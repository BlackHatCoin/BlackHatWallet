// Copyright (c) 2019 The Dash Core developers
// Copyright (c) 2023 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "quorums.h"
#include "quorums_chainlocks.h"
#include "quorums_signing.h"
#include "quorums_utils.h"

#include "chain.h"
#include "net_processing.h"
#include "scheduler.h"
#include "spork.h"
#include "sporkid.h"
#include "validation.h"

namespace llmq
{

static const std::string CLSIG_REQUESTID_PREFIX = "clsig";

CChainLocksHandler* chainLocksHandler;

std::string CChainLockSig::ToString() const
{
    return strprintf("CChainLockSig(nHeight=%d, blockHash=%s)", nHeight, blockHash.ToString());
}

CChainLocksHandler::CChainLocksHandler(CScheduler* _scheduler) :
    scheduler(_scheduler)
{
    quorumSigningManager->RegisterRecoveredSigsListener(this);
}

CChainLocksHandler::~CChainLocksHandler()
{
    quorumSigningManager->UnregisterRecoveredSigsListener(this);
}

bool CChainLocksHandler::AlreadyHave(const CInv& inv)
{
    LOCK(cs);
    return seenChainLocks.count(inv.hash) != 0;
}

bool CChainLocksHandler::GetChainLockByHash(const uint256& hash, llmq::CChainLockSig& ret)
{
    LOCK(cs);

    if (hash != bestChainLockHash) {
        // we only propagate the best one and ditch all the old ones
        return false;
    }

    ret = bestChainLock;
    return true;
}

void CChainLocksHandler::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    if (strCommand == NetMsgType::CLSIG) {
        CChainLockSig clsig;
        vRecv >> clsig;

        auto hash = ::SerializeHash(clsig);

        {
            LOCK(cs_main);
            connman.RemoveAskFor(hash, MSG_CLSIG);
        }

        ProcessNewChainLock(pfrom->GetId(), clsig, hash);
    }
}

void CChainLocksHandler::ProcessNewChainLock(NodeId from, const llmq::CChainLockSig& clsig, const uint256& hash)
{
    {
        LOCK(cs);
        if (!seenChainLocks.emplace(hash, GetTimeMillis()).second) {
            return;
        }

        if (bestChainLock.nHeight != -1 && clsig.nHeight <= bestChainLock.nHeight) {
            // no need to process/relay older CLSIGs
            return;
        }
    }

    uint256 requestId = ::SerializeHash(std::make_pair(CLSIG_REQUESTID_PREFIX, clsig.nHeight));
    uint256 msgHash = clsig.blockHash;
    if (!quorumSigningManager->VerifyRecoveredSig(Params().GetConsensus().llmqChainLocks, clsig.nHeight, requestId, msgHash, clsig.sig)) {
        LogPrintf("CChainLocksHandler::%s -- invalid CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
        if (from != -1) {
            LOCK(cs_main);
            Misbehaving(from, 100);
        }
        return;
    }

    {
        LOCK2(cs_main, cs);

        if (InternalHasConflictingChainLock(clsig.nHeight, clsig.blockHash)) {
            // This should not happen. If it happens, it means that a malicious entity controls a large part of the MN
            // network. In this case, we don't allow him to reorg older chainlocks.
            LogPrintf("CChainLocksHandler::%s -- new CLSIG (%s) tries to reorg previous CLSIG (%s), peer=%d\n",
                      __func__, clsig.ToString(), bestChainLock.ToString(), from);
            return;
        }

        bestChainLockHash = hash;
        bestChainLock = clsig;

        CInv inv(MSG_CLSIG, hash);
        g_connman->RelayInv(inv);

        auto blockIt = mapBlockIndex.find(clsig.blockHash);
        if (blockIt == mapBlockIndex.end()) {
            // we don't know the block/header for this CLSIG yet, so bail out for now
            // when the block or the header later comes in, we will enforce the correct chain
            return;
        }

        if (blockIt->second->nHeight != clsig.nHeight) {
            // Should not happen, same as the conflict check from above.
            LogPrintf("CChainLocksHandler::%s -- height of CLSIG (%s) does not match the specified block's height (%d)\n",
                    __func__, clsig.ToString(), blockIt->second->nHeight);
            return;
        }

        const CBlockIndex* pindex = blockIt->second;
        bestChainLockWithKnownBlock = bestChainLock;
        bestChainLockBlockIndex = pindex;
    }

    EnforceBestChainLock();

    LogPrintf("CChainLocksHandler::%s -- processed new CLSIG (%s), peer=%d\n",
              __func__, clsig.ToString(), from);
}

void CChainLocksHandler::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    bool doEnforce = false;
    {
        LOCK2(cs_main, cs);

        if (pindexNew->GetBlockHash() == bestChainLock.blockHash) {
            LogPrintf("CChainLocksHandler::%s -- block header %s came in late, updating and enforcing\n", __func__, pindexNew->GetBlockHash().ToString());

            if (bestChainLock.nHeight != pindexNew->nHeight) {
                // Should not happen, same as the conflict check from ProcessNewChainLock.
                LogPrintf("CChainLocksHandler::%s -- height of CLSIG (%s) does not match the specified block's height (%d)\n",
                          __func__, bestChainLock.ToString(), pindexNew->nHeight);
                return;
            }

            bestChainLockBlockIndex = pindexNew;
            doEnforce = true;
        }
    }
    if (doEnforce) {
        EnforceBestChainLock();
    }
}

void CChainLocksHandler::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork)
{
    if (!fMasterNode) {
        return;
    }
    if (!pindexNew->pprev) {
        return;
    }
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    // DIP8 defines a process called "Signing attempts" which should run before the CLSIG is finalized
    // To simplify the initial implementation, we skip this process and directly try to create a CLSIG
    // This will fail when multiple blocks compete, but we accept this for the initial implementation.
    // Later, we'll add the multiple attempts process.

    uint256 requestId = ::SerializeHash(std::make_pair(CLSIG_REQUESTID_PREFIX, pindexNew->nHeight));
    uint256 msgHash = pindexNew->GetBlockHash();

    {
        LOCK(cs);

        if (InternalHasConflictingChainLock(pindexNew->nHeight, pindexNew->GetBlockHash())) {
            if (!inEnforceBestChainLock) {
                // we accepted this block when there was no lock yet, but now a conflicting lock appeared. Invalidate it.
                LogPrintf("CChainLocksHandler::%s -- conflicting lock after block was accepted, invalidating now\n",
                          __func__);
                ScheduleInvalidateBlock(pindexNew);
            }
            return;
        }

        if (bestChainLock.nHeight >= pindexNew->nHeight) {
            // already got the same CLSIG or a better one
            return;
        }

        if (pindexNew->nHeight == lastSignedHeight) {
            // already signed this one
            return;
        }
        lastSignedHeight = pindexNew->nHeight;
        lastSignedRequestId = requestId;
        lastSignedMsgHash = msgHash;
    }

    quorumSigningManager->AsyncSignIfMember(Params().GetConsensus().llmqChainLocks, requestId, msgHash);

    Cleanup();
}

// WARNING: cs_main and cs should not be held!
void CChainLocksHandler::EnforceBestChainLock()
{
    CChainLockSig clsig;
    const CBlockIndex* pindex;
    {
        LOCK(cs);
        clsig = bestChainLockWithKnownBlock;
        pindex = bestChainLockBlockIndex;
    }

    {
        LOCK(cs_main);

        // Go backwards through the chain referenced by clsig until we find a block that is part of the main chain.
        // For each of these blocks, check if there are children that are NOT part of the chain referenced by clsig
        // and invalidate each of them.
        inEnforceBestChainLock = true; // avoid unnecessary ScheduleInvalidateBlock calls inside UpdatedBlockTip
        while (pindex && !chainActive.Contains(pindex)) {
            // Invalidate all blocks that have the same prevBlockHash but are not equal to blockHash
            auto itp = mapPrevBlockIndex.equal_range(pindex->pprev->GetBlockHash());
            for (auto jt = itp.first; jt != itp.second; ++jt) {
                if (jt->second == pindex) {
                    continue;
                }
                LogPrintf("CChainLocksHandler::%s -- CLSIG (%s) invalidates block %s\n",
                          __func__, bestChainLockWithKnownBlock.ToString(), jt->second->GetBlockHash().ToString());
                DoInvalidateBlock(jt->second, false);
            }

            pindex = pindex->pprev;
        }
        inEnforceBestChainLock = false;
    }

    CValidationState state;
    if (!ActivateBestChain(state)) {
        LogPrintf("CChainLocksHandler::UpdatedBlockTip -- ActivateBestChain failed: %s\n", state.GetRejectReason());
        // This should not have happened and we are in a state were it's not safe to continue anymore
        assert(false);
    }
}

void CChainLocksHandler::HandleNewRecoveredSig(const llmq::CRecoveredSig& recoveredSig)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    CChainLockSig clsig;
    {
        LOCK(cs);

        if (recoveredSig.id != lastSignedRequestId || recoveredSig.msgHash != lastSignedMsgHash) {
            // this is not what we signed, so lets not create a CLSIG for it
            return;
        }
        if (bestChainLock.nHeight >= lastSignedHeight) {
            // already got the same or a better CLSIG through the CLSIG message
            return;
        }

        clsig.nHeight = lastSignedHeight;
        clsig.blockHash = lastSignedMsgHash;
        clsig.sig = recoveredSig.sig;
    }
    ProcessNewChainLock(-1, clsig, ::SerializeHash(clsig));
}

void CChainLocksHandler::ScheduleInvalidateBlock(const CBlockIndex* pindex)
{
    // Calls to InvalidateBlock and ActivateBestChain might result in re-invocation of the UpdatedBlockTip and other
    // signals, so we can't directly call it from signal handlers. We solve this by doing the call from the scheduler

    scheduler->scheduleFromNow([this, pindex]() {
        DoInvalidateBlock(pindex, true);
    }, 0);
}

// WARNING, do not hold cs while calling this method as we'll otherwise run into a deadlock
void CChainLocksHandler::DoInvalidateBlock(const CBlockIndex* pindex, bool activateBestChain)
{
    auto& params = Params();

    {
        LOCK(cs_main);

        // get the non-const pointer
        CBlockIndex* pindex2 = mapBlockIndex[pindex->GetBlockHash()];

        CValidationState state;
        if (!InvalidateBlock(state, params, pindex2)) {
            LogPrintf("CChainLocksHandler::UpdatedBlockTip -- InvalidateBlock failed: %s\n", state.GetRejectReason());
            // This should not have happened and we are in a state were it's not safe to continue anymore
            assert(false);
        }
    }

    CValidationState state;
    if (activateBestChain && !ActivateBestChain(state)) {
        LogPrintf("CChainLocksHandler::UpdatedBlockTip -- ActivateBestChain failed: %s\n", state.GetRejectReason());
        // This should not have happened and we are in a state were it's not safe to continue anymore
        assert(false);
    }
}

bool CChainLocksHandler::HasChainLock(int nHeight, const uint256& blockHash)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return false;
    }

    LOCK(cs);
    return InternalHasChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash == bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    return pAncestor && pAncestor->GetBlockHash() == blockHash;
}

bool CChainLocksHandler::HasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return false;
    }

    LOCK(cs);
    return InternalHasConflictingChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash != bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    assert(pAncestor);
    return pAncestor->GetBlockHash() != blockHash;
}

void CChainLocksHandler::Cleanup()
{
    {
        LOCK(cs);
        if (GetTimeMillis() - lastCleanupTime < CLEANUP_INTERVAL) {
            return;
        }
    }

    LOCK2(cs_main, cs);

    for (auto it = seenChainLocks.begin(); it != seenChainLocks.end(); ) {
        if (GetTimeMillis() - it->second >= CLEANUP_SEEN_TIMEOUT) {
            it = seenChainLocks.erase(it);
        } else {
            ++it;
        }
    }

    lastCleanupTime = GetTimeMillis();
}

}