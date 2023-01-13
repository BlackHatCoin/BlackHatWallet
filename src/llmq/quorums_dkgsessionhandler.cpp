// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_dkgsessionhandler.h"

#include "activemasternode.h"
#include "chainparams.h"
#include "llmq/quorums_blockprocessor.h"
#include "llmq/quorums_connections.h"
#include "llmq/quorums_debug.h"
#include "net_processing.h"
#include "shutdown.h"
#include "util/threadnames.h"
#include "validation.h"

namespace llmq
{

CDKGPendingMessages::CDKGPendingMessages(size_t _maxMessagesPerNode) :
    maxMessagesPerNode(_maxMessagesPerNode)
{
}

void CDKGPendingMessages::PushPendingMessage(NodeId from, CDataStream& vRecv, int invType)
{
    // this will also consume the data, even if we bail out early
    auto pm = std::make_shared<CDataStream>(std::move(vRecv));

    {
        LOCK(cs);

        if (messagesPerNode[from] >= maxMessagesPerNode) {
            // TODO ban?
            LogPrint(BCLog::NET, "CDKGPendingMessages::%s -- too many messages, peer=%d\n", __func__, from);
            return;
        }
        messagesPerNode[from]++;
    }

    CHashWriter hw(SER_GETHASH, 0);
    hw.write(pm->data(), pm->size());
    uint256 hash = hw.GetHash();

    LOCK2(cs_main, cs);

    g_connman->RemoveAskFor(hash, invType);

    if (!seenMessages.emplace(hash).second) {
        LogPrint(BCLog::NET, "CDKGPendingMessages::%s -- already seen %s, peer=%d\n", __func__, hash.ToString(), from);
        return;
    }

    pendingMessages.emplace_back(std::make_pair(from, std::move(pm)));
}

std::list<CDKGPendingMessages::BinaryMessage> CDKGPendingMessages::PopPendingMessages(size_t maxCount)
{
    LOCK(cs);

    std::list<BinaryMessage> ret;
    while (!pendingMessages.empty() && ret.size() < maxCount) {
        ret.emplace_back(std::move(pendingMessages.front()));
        pendingMessages.pop_front();
    }

    return ret;
}

bool CDKGPendingMessages::HasSeen(const uint256& hash) const
{
    LOCK(cs);
    return seenMessages.count(hash) != 0;
}

void CDKGPendingMessages::Clear()
{
    LOCK(cs);
    pendingMessages.clear();
    messagesPerNode.clear();
    seenMessages.clear();
}

//////

CDKGSessionHandler::CDKGSessionHandler(const Consensus::LLMQParams& _params, CEvoDB& _evoDb, CBLSWorker& _blsWorker, CDKGSessionManager& _dkgManager) :
    params(_params),
    evoDb(_evoDb),
    blsWorker(_blsWorker),
    dkgManager(_dkgManager),
    curSession(std::make_shared<CDKGSession>(_params, _evoDb, _blsWorker, _dkgManager)),
    pendingContributions((size_t)_params.size * 2), // we allow size*2 messages as we need to make sure we see bad behavior (double messages)
    pendingComplaints((size_t)_params.size * 2),
    pendingJustifications((size_t)_params.size * 2),
    pendingPrematureCommitments((size_t)_params.size * 2)
{
    if (params.type == Consensus::LLMQ_NONE) {
        throw std::runtime_error("Can't initialize CDKGSessionHandler with LLMQ_NONE type.");
    }
}

CDKGSessionHandler::~CDKGSessionHandler()
{
}

void CDKGSessionHandler::UpdatedBlockTip(const CBlockIndex* pindexNew)
{
    AssertLockHeld(cs_main);
    LOCK(cs);

    int quorumStageInt = pindexNew->nHeight % params.dkgInterval;
    const CBlockIndex* pindexQuorum = chainActive[pindexNew->nHeight - quorumStageInt];

    currentHeight = pindexNew->nHeight;
    quorumHeight = pindexQuorum->nHeight;
    quorumHash = pindexQuorum->GetBlockHash();

    bool fNewPhase = (quorumStageInt % params.dkgPhaseBlocks) == 0;
    int phaseInt = quorumStageInt / params.dkgPhaseBlocks + 1;
    QuorumPhase oldPhase = phase;

    if (fNewPhase && phaseInt >= QuorumPhase_Initialized && phaseInt <= QuorumPhase_Idle) {
        phase = static_cast<QuorumPhase>(phaseInt);
    }

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - currentHeight=%d, quorumHeight=%d, oldPhase=%d, newPhase=%d\n", __func__,
            params.name, currentHeight, quorumHeight, oldPhase, phase);
}

void CDKGSessionHandler::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    // We don't handle messages in the calling thread as deserialization/processing of these would block everything
    if (strCommand == NetMsgType::QCONTRIB) {
        pendingContributions.PushPendingMessage(pfrom->GetId(), vRecv, MSG_QUORUM_CONTRIB);
    } else if (strCommand == NetMsgType::QCOMPLAINT) {
        pendingComplaints.PushPendingMessage(pfrom->GetId(), vRecv, MSG_QUORUM_COMPLAINT);
    } else if (strCommand == NetMsgType::QJUSTIFICATION) {
        pendingJustifications.PushPendingMessage(pfrom->GetId(), vRecv, MSG_QUORUM_JUSTIFICATION);
    } else if (strCommand == NetMsgType::QPCOMMITMENT) {
        pendingPrematureCommitments.PushPendingMessage(pfrom->GetId(), vRecv, MSG_QUORUM_PREMATURE_COMMITMENT);
    }
}

void CDKGSessionHandler::StartThread()
{
    if (phaseHandlerThread.joinable()) {
        throw std::runtime_error("Tried to start an already started CDKGSessionHandler thread.");
    }

    std::string threadName = strprintf("quorum-phase-%d", params.type);
    phaseHandlerThread = std::thread(&TraceThread<std::function<void()> >, threadName, std::function<void()>(std::bind(&CDKGSessionHandler::PhaseHandlerThread, this)));
}

void CDKGSessionHandler::StopThread()
{
    stopRequested = true;
    if (phaseHandlerThread.joinable()) {
        phaseHandlerThread.join();
    }
}

bool CDKGSessionHandler::InitNewQuorum(const CBlockIndex* pindexQuorum)
{
    curSession = std::make_shared<CDKGSession>(params, evoDb, blsWorker, dkgManager);

    if (!deterministicMNManager->IsDIP3Enforced(pindexQuorum->nHeight) ||
            !activeMasternodeManager) {
        return false;
    }

    auto mns = deterministicMNManager->GetAllQuorumMembers(params.type, pindexQuorum);

    if (!curSession->Init(pindexQuorum, mns, activeMasternodeManager->GetProTx())) {
        LogPrintf("CDKGSessionHandler::%s -- quorum initialiation failed for %s\n", __func__, curSession->params.name);
        return false;
    }

    return true;
}

CDKGSessionHandler::QuorumPhaseAndHash CDKGSessionHandler::GetPhaseAndQuorumHash() const
{
    LOCK(cs);
    return {phase, quorumHash};
}

class AbortPhaseException : public std::exception {
};

void CDKGSessionHandler::WaitForNextPhase(QuorumPhase curPhase,
                                          QuorumPhase nextPhase,
                                          const uint256& expectedQuorumHash,
                                          const WhileWaitFunc& runWhileWaiting)
{
    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - starting, curPhase=%d, nextPhase=%d\n", __func__, params.name, curPhase, nextPhase);

    while (true) {
        if (stopRequested || ShutdownRequested()) {
            throw AbortPhaseException();
        }
        auto currState = GetPhaseAndQuorumHash();
        if (!expectedQuorumHash.IsNull() && currState.quorumHash != expectedQuorumHash) {
            throw AbortPhaseException();
        }
        if (currState.phase == nextPhase) {
            break;
        }
        if (curPhase != QuorumPhase_None && currState.phase != curPhase) {
            LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - aborting due unexpected phase change\n", __func__, params.name);
            throw AbortPhaseException();
        }
        if (!runWhileWaiting()) {
            MilliSleep(100);
        }
    }

    if (nextPhase == QuorumPhase_Initialized) {
        quorumDKGDebugManager->ResetLocalSessionStatus(params.type);
    } else {
        quorumDKGDebugManager->UpdateLocalSessionStatus(params.type, [&](CDKGDebugSessionStatus& status) {
            bool changed = status.phase != (uint8_t) nextPhase;
            status.phase = (uint8_t) nextPhase;
            return changed;
        });
    }

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - done, curPhase=%d, nextPhase=%d\n", __func__, params.name, curPhase, nextPhase);

}

void CDKGSessionHandler::WaitForNewQuorum(const uint256& oldQuorumHash)
{
    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - starting\n", __func__, params.name);

    while (true) {
        if (stopRequested || ShutdownRequested()) {
            throw AbortPhaseException();
        }
        auto currState = GetPhaseAndQuorumHash();
        if (currState.quorumHash != oldQuorumHash) {
            break;
        }
        MilliSleep(100);
    }

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - done\n", __func__, params.name);
}

// Sleep some time to not fully overload the whole network
void CDKGSessionHandler::SleepBeforePhase(QuorumPhase curPhase,
                                          const uint256& expectedQuorumHash,
                                          double randomSleepFactor,
                                          const WhileWaitFunc& runWhileWaiting)
{
    if (Params().IsRegTestNet()) {
        // On regtest, blocks can be mined on demand without any significant time passing between these.
        // We shouldn't wait before phases in this case.
        return;
    }

    if (!curSession->AreWeMember()) {
        // Non-members do not participate and do not create any network load, no need to sleep.
        return;
    }

    // Two blocks can come very close to each other, this happens pretty regularly. We don't want to be
    // left behind and marked as a bad member. This means that we should not count the last block of the
    // phase as a safe one to keep sleeping, that's why we calculate the phase sleep time as a time of
    // the full phase minus one block here.
    const int nTargetSpacing = Params().GetConsensus().nTargetSpacing;
    double phaseSleepTime = (params.dkgPhaseBlocks - 1) * nTargetSpacing * 1000;
    // Expected phase sleep time per member
    double phaseSleepTimePerMember = phaseSleepTime / params.size;
    // Don't expect perfect block times and thus reduce the phase time to be on the secure side (caller chooses factor)
    double adjustedPhaseSleepTimePerMember = phaseSleepTimePerMember * randomSleepFactor;

    int64_t sleepTime = (int64_t)(adjustedPhaseSleepTimePerMember * curSession->GetMyMemberIndex());
    int64_t endTime = GetTimeMillis() + sleepTime;
    int heightTmp{-1};
    int heightStart{-1};
    {
        LOCK(cs);
        heightTmp = heightStart = currentHeight;
    }

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - starting sleep for %d ms, curPhase=%d\n", __func__, params.name, sleepTime, curPhase);

    while (GetTimeMillis() < endTime) {
        if (stopRequested) {
            LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - aborting due to stop/shutdown requested\n", __func__, params.name);
            throw AbortPhaseException();
        }
        {
            LOCK(cs);
            if (currentHeight > heightTmp) {
                // New block(s) just came in
                int64_t expectedBlockTime = (currentHeight - heightStart) * nTargetSpacing * 1000;
                if (expectedBlockTime > sleepTime) {
                    // Blocks came faster than we expected, jump into the phase func asap
                    break;
                }
                heightTmp = currentHeight;
            }
            if (phase != curPhase || quorumHash != expectedQuorumHash) {
                // Something went wrong and/or we missed quite a few blocks and it's just too late now
                LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - aborting due unexpected phase/expectedQuorumHash change\n", __func__, params.name);
                throw AbortPhaseException();
            }
        }
        if (!runWhileWaiting()) {
            MilliSleep(100);
        }
    }

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - done, curPhase=%d\n", __func__, params.name, curPhase);
}

void CDKGSessionHandler::HandlePhase(QuorumPhase curPhase,
                                     QuorumPhase nextPhase,
                                     const uint256& expectedQuorumHash,
                                     double randomSleepFactor,
                                     const StartPhaseFunc& startPhaseFunc,
                                     const WhileWaitFunc& runWhileWaiting)
{
    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - starting, curPhase=%d, nextPhase=%d\n", __func__, params.name, curPhase, nextPhase);

    SleepBeforePhase(curPhase, expectedQuorumHash, randomSleepFactor, runWhileWaiting);
    startPhaseFunc();
    WaitForNextPhase(curPhase, nextPhase, expectedQuorumHash, runWhileWaiting);

    LogPrint(BCLog::DKG, "CDKGSessionHandler::%s -- %s - done, curPhase=%d, nextPhase=%d\n", __func__, params.name, curPhase, nextPhase);
}

// returns a set of NodeIds which sent invalid messages
template<typename Message>
std::set<NodeId> BatchVerifyMessageSigs(CDKGSession& session, const std::vector<std::pair<NodeId, std::shared_ptr<Message>>>& messages)
{
    if (messages.empty()) {
        return {};
    }

    std::set<NodeId> ret;
    bool revertToSingleVerification = false;

    CBLSSignature aggSig;
    std::vector<CBLSPublicKey> pubKeys;
    std::vector<uint256> messageHashes;
    std::set<uint256> messageHashesSet;
    pubKeys.reserve(messages.size());
    messageHashes.reserve(messages.size());
    bool first = true;
    for (const auto& p : messages ) {
        const auto& msg = *p.second;

        auto member = session.GetMember(msg.proTxHash);
        if (!member) {
            // should not happen as it was verified before
            ret.emplace(p.first);
            continue;
        }

        if (first) {
            aggSig = msg.sig;
        } else {
            aggSig.AggregateInsecure(msg.sig);
        }
        first = false;

        auto msgHash = msg.GetSignHash();
        if (!messageHashesSet.emplace(msgHash).second) {
            // can only happen in 2 cases:
            // 1. Someone sent us the same message twice but with differing signature, meaning that at least one of them
            //    must be invalid. In this case, we'd have to revert to single message verification nevertheless
            // 2. Someone managed to find a way to create two different binary representations of a message that deserializes
            //    to the same object representation. This would be some form of malleability. However, this shouldn't be
            //    possible as only deterministic/unique BLS signatures and very simple data types are involved
            revertToSingleVerification = true;
            break;
        }

        pubKeys.emplace_back(member->dmn->pdmnState->pubKeyOperator.Get());
        messageHashes.emplace_back(msgHash);
    }
    if (!revertToSingleVerification) {
        bool valid = aggSig.VerifyInsecureAggregated(pubKeys, messageHashes);
        if (valid) {
            // all good
            return ret;
        }

        // are all messages from the same node?
        NodeId firstNodeId;
        first = true;
        bool nodeIdsAllSame = true;
        for (auto it = messages.begin(); it != messages.end(); ++it) {
            if (first) {
                firstNodeId = it->first;
            } else {
                first = false;
                if (it->first != firstNodeId) {
                    nodeIdsAllSame = false;
                    break;
                }
            }
        }
        // if yes, take a short path and return a set with only him
        if (nodeIdsAllSame) {
            ret.emplace(firstNodeId);
            return ret;
        }
        // different nodes, let's figure out who are the bad ones
    }

    for (const auto& p : messages) {
        if (ret.count(p.first)) {
            continue;
        }

        const auto& msg = *p.second;
        auto member = session.GetMember(msg.proTxHash);
        bool valid = msg.sig.VerifyInsecure(member->dmn->pdmnState->pubKeyOperator.Get(), msg.GetSignHash());
        if (!valid) {
            ret.emplace(p.first);
        }
    }
    return ret;
}

template<typename Message>
static bool ProcessPendingMessageBatch(CDKGSession& session, CDKGPendingMessages& pendingMessages, size_t maxCount)
{
    auto msgs = pendingMessages.PopAndDeserializeMessages<Message>(maxCount);
    if (msgs.empty()) {
        return false;
    }

    std::vector<uint256> hashes;
    std::vector<std::pair<NodeId, std::shared_ptr<Message>>> preverifiedMessages;
    hashes.reserve(msgs.size());
    preverifiedMessages.reserve(msgs.size());

    for (const auto& p : msgs) {
        if (!p.second) {
            LogPrint(BCLog::NET, "%s -- failed to deserialize message, peer=%d\n", __func__, p.first);
            {
                LOCK(cs_main);
                Misbehaving(p.first, 100);
            }
            continue;
        }
        const auto& msg = *p.second;

        bool ban = false;
        if (!session.PreVerifyMessage(msg, ban)) {
            if (ban) {
                LogPrint(BCLog::NET, "%s -- banning node due to failed preverification, peer=%d\n", __func__, p.first);
                {
                    LOCK(cs_main);
                    Misbehaving(p.first, 100);
                }
            }
            LogPrint(BCLog::NET, "%s -- skipping message due to failed preverification, peer=%d\n", __func__, p.first);
            continue;
        }
        hashes.emplace_back(::SerializeHash(msg));
        preverifiedMessages.emplace_back(p);
    }
    if (preverifiedMessages.empty()) {
        return true;
    }

    auto badNodes = BatchVerifyMessageSigs(session, preverifiedMessages);
    if (!badNodes.empty()) {
        LOCK(cs_main);
        for (auto nodeId : badNodes) {
            LogPrint(BCLog::NET, "%s -- failed to verify signature, peer=%d\n", __func__, nodeId);
            Misbehaving(nodeId, 100);
        }
    }

    for (size_t i = 0; i < preverifiedMessages.size(); i++) {
        NodeId nodeId = preverifiedMessages[i].first;
        if (badNodes.count(nodeId)) {
            continue;
        }
        const auto& msg = *preverifiedMessages[i].second;
        bool ban = false;
        session.ReceiveMessage(hashes[i], msg, ban);
        if (ban) {
            LogPrint(BCLog::NET, "%s -- banning node after ReceiveMessage failed, peer=%d\n", __func__, nodeId);
            LOCK(cs_main);
            Misbehaving(nodeId, 100);
            badNodes.emplace(nodeId);
        }
    }

    return true;
}

void CDKGSessionHandler::HandleDKGRound()
{
    uint256 curQuorumHash;

    WaitForNextPhase(QuorumPhase_None, QuorumPhase_Initialized, UINT256_ZERO, []{return false;});

    {
        LOCK(cs);
        pendingContributions.Clear();
        pendingComplaints.Clear();
        pendingJustifications.Clear();
        pendingPrematureCommitments.Clear();
        curQuorumHash = quorumHash;
    }

    const CBlockIndex* pindexQuorum = WITH_LOCK(cs_main, return LookupBlockIndex(curQuorumHash));
    if (!pindexQuorum) {
        // should never happen
        LogPrintf("%s: ERROR: Unable to find block %s\n", __func__, curQuorumHash.ToString());
        return;
    }

    if (!InitNewQuorum(pindexQuorum)) {
        // should actually never happen
        WaitForNewQuorum(curQuorumHash);
        throw AbortPhaseException();
    }

    quorumDKGDebugManager->UpdateLocalSessionStatus(params.type, [&](CDKGDebugSessionStatus& status) {
        bool changed = status.phase != (uint8_t) QuorumPhase_Initialized;
        status.phase = (uint8_t) QuorumPhase_Initialized;
        return changed;
    });

    EnsureQuorumConnections(params.type, pindexQuorum, curSession->myProTxHash);
    if (curSession->AreWeMember()) {
        AddQuorumProbeConnections(params.type, pindexQuorum, curSession->myProTxHash);
    }

    WaitForNextPhase(QuorumPhase_Initialized, QuorumPhase_Contribute, curQuorumHash, []{return false;});

    // Contribute
    auto fContributeStart = [this]() {
        curSession->Contribute(pendingContributions);
    };
    auto fContributeWait = [this] {
        return ProcessPendingMessageBatch<CDKGContribution>(*curSession, pendingContributions, 8);
    };
    HandlePhase(QuorumPhase_Contribute, QuorumPhase_Complain, curQuorumHash, 0.05, fContributeStart, fContributeWait);

    // Complain
    auto fComplainStart = [this]() {
        curSession->VerifyAndComplain(pendingComplaints);
    };
    auto fComplainWait = [this] {
        return ProcessPendingMessageBatch<CDKGComplaint>(*curSession, pendingComplaints, 8);
    };
    HandlePhase(QuorumPhase_Complain, QuorumPhase_Justify, curQuorumHash, 0.05, fComplainStart, fComplainWait);

    // Justify
    auto fJustifyStart = [this]() {
        curSession->VerifyAndJustify(pendingJustifications);
    };
    auto fJustifyWait = [this] {
        return ProcessPendingMessageBatch<CDKGJustification>(*curSession, pendingJustifications, 8);
    };
    HandlePhase(QuorumPhase_Justify, QuorumPhase_Commit, curQuorumHash, 0.05, fJustifyStart, fJustifyWait);

    // Commit
    auto fCommitStart = [this]() {
        curSession->VerifyAndCommit(pendingPrematureCommitments);
    };
    auto fCommitWait = [this] {
        return ProcessPendingMessageBatch<CDKGPrematureCommitment>(*curSession, pendingPrematureCommitments, 8);
    };
    HandlePhase(QuorumPhase_Commit, QuorumPhase_Finalize, curQuorumHash, 0.1, fCommitStart, fCommitWait);

    auto finalCommitments = curSession->FinalizeCommitments();
    for (const auto& fqc : finalCommitments) {
        quorumBlockProcessor->AddAndRelayMinableCommitment(fqc);
    }
}

void CDKGSessionHandler::PhaseHandlerThread()
{
    while (!stopRequested && !ShutdownRequested()) {
        try {
            HandleDKGRound();
        } catch (AbortPhaseException& e) {
            quorumDKGDebugManager->UpdateLocalSessionStatus(params.type, [&](CDKGDebugSessionStatus& status) {
                status.aborted = true;
                return true;
            });
            LogPrintf("CDKGSessionHandler::%s -- aborted current DKG session\n", __func__);
        }
    }
}

}
