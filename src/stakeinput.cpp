// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2021 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "stakeinput.h"

#include "chain.h"
#include "txdb.h"
#include "validation.h"

static bool HasStakeMinAgeOrDepth(int nHeight, uint32_t nTime, const CBlockIndex* pindex)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (consensus.NetworkUpgradeActive(nHeight + 1, Consensus::UPGRADE_ZC_PUBLIC) &&
            !consensus.HasStakeMinAgeOrDepth(nHeight, nTime, pindex->nHeight, pindex->nTime)) {
        return error("%s : min age violation - height=%d - time=%d, nHeightBlockFrom=%d, nTimeBlockFrom=%d",
                     __func__, nHeight, nTime, pindex->nHeight, pindex->nTime);
    }
    return true;
}

CBlkcStake* CBlkcStake::NewBlkcStake(const CTxIn& txin, int nHeight, uint32_t nTime)
{
    if (txin.IsZerocoinSpend()) {
        error("%s: unable to initialize CBlkcStake from zerocoin spend", __func__);
        return nullptr;
    }

    // Look for the stake input in the coins cache first
    const Coin& coin = pcoinsTip->AccessCoin(txin.prevout);
    if (!coin.IsSpent()) {
        const CBlockIndex* pindexFrom = mapBlockIndex.at(chainActive[coin.nHeight]->GetBlockHash());
        // Check that the stake has the required depth/age
        if (!HasStakeMinAgeOrDepth(nHeight, nTime, pindexFrom)) {
            return nullptr;
        }
        // All good
        return new CBlkcStake(coin.out, txin.prevout, pindexFrom);
    }

    // Otherwise find the previous transaction in database
    uint256 hashBlock;
    CTransactionRef txPrev;
    if (!GetTransaction(txin.prevout.hash, txPrev, hashBlock, true)) {
        error("%s : INFO: read txPrev failed, tx id prev: %s", __func__, txin.prevout.hash.GetHex());
        return nullptr;
    }
    const CBlockIndex* pindexFrom = nullptr;
    if (mapBlockIndex.count(hashBlock)) {
        CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
        if (chainActive.Contains(pindex)) pindexFrom = pindex;
    }
    // Check that the input is in the active chain
    if (!pindexFrom) {
        error("%s : Failed to find the block index for stake origin", __func__);
        return nullptr;
    }
    // Check that the stake has the required depth/age
    if (!HasStakeMinAgeOrDepth(nHeight, nTime, pindexFrom)) {
        return nullptr;
    }
    // All good
    return new CBlkcStake(txPrev->vout[txin.prevout.n], txin.prevout, pindexFrom);
}

bool CBlkcStake::GetTxOutFrom(CTxOut& out) const
{
    out = outputFrom;
    return true;
}

CTxIn CBlkcStake::GetTxIn() const
{
    return CTxIn(outpointFrom.hash, outpointFrom.n);
}

CAmount CBlkcStake::GetValue() const
{
    return outputFrom.nValue;
}

CDataStream CBlkcStake::GetUniqueness() const
{
    //The unique identifier for a BLKC stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << outpointFrom.n << outpointFrom.hash;
    return ss;
}

//The block that the UTXO was added to the chain
const CBlockIndex* CBlkcStake::GetIndexFrom() const
{
    // Sanity check, pindexFrom is set on the constructor.
    if (!pindexFrom) throw std::runtime_error("CBlkcStake: uninitialized pindexFrom");
    return pindexFrom;
}

