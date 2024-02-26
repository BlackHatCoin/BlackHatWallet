// Copyright (c) 2018-2022 The Dash Core developers
// Copyright (c) 2023 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_QUORUMS_SIGNING_H
#define BLKC_QUORUMS_SIGNING_H

#include "llmq/quorums.h"

#include "chainparams.h"
#include "net.h"
#include "sync.h"

namespace llmq
{

class CRecoveredSig
{
public:
    uint8_t llmqType;
    uint256 quorumHash;
    uint256 id;
    uint256 msgHash;
    CBLSSignature sig;

    // only in-memory
    uint256 hash;

public:
    template <typename Stream>
    inline void Serialize(Stream& s) const
    {
        s << llmqType;
        s << quorumHash;
        s << id;
        s << msgHash;
        s << sig;
    }
    template <typename Stream>
    inline void Unserialize(Stream& s, bool checkMalleable = true, bool updateHash = true, bool skipSig = false)
    {
        s >> llmqType;
        s >> quorumHash;
        s >> id;
        s >> msgHash;
        if (!skipSig) {
            sig.Unserialize(s, checkMalleable);
            if (updateHash) {
                UpdateHash();
            }
        }
    }

    void UpdateHash()
    {
        hash = ::SerializeHash(*this);
    }

    const uint256& GetHash() const
    {
        assert(!hash.IsNull());
        return hash;
    }
};

// TODO implement caching to speed things up
class CRecoveredSigsDb
{
private:
    CDBWrapper db;

public:
    CRecoveredSigsDb(bool fMemory);

    bool HasRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);
    bool HasRecoveredSigForId(Consensus::LLMQType llmqType, const uint256& id);
    bool HasRecoveredSigForSession(const uint256& signHash);
    bool HasRecoveredSigForHash(const uint256& hash);
    bool GetRecoveredSigByHash(const uint256& hash, CRecoveredSig& ret);
    bool GetRecoveredSigById(Consensus::LLMQType llmqType, const uint256& id, CRecoveredSig& ret);
    void WriteRecoveredSig(const CRecoveredSig& recSig);

    void CleanupOldRecoveredSigs(int64_t maxAge);

    // votes are removed when the recovered sig is written to the db
    bool HasVotedOnId(Consensus::LLMQType llmqType, const uint256& id);
    bool GetVoteForId(Consensus::LLMQType llmqType, const uint256& id, uint256& msgHashRet);
    void WriteVoteForId(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);

private:
    bool ReadRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, CRecoveredSig& ret);
};

class CRecoveredSigsListener
{
public:
    virtual ~CRecoveredSigsListener() {}

    virtual void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) = 0;
};

class CSigningManager
{
    friend class CSigSharesManager;
    static const int64_t DEFAULT_MAX_RECOVERED_SIGS_AGE = 60 * 60 * 24 * 7; // keep them for a week

    // when selecting a quorum for signing and verification, we use CQuorumManager::SelectQuorum with this offset as
    // starting height for scanning. This is because otherwise the resulting signatures would not be verifiable by nodes
    // which are not 100% at the chain tip.
    static const int SIGN_HEIGHT_OFFSET = 8;

private:
    RecursiveMutex cs;

    CRecoveredSigsDb db;

    // Incoming and not verified yet
    std::map<NodeId, std::list<CRecoveredSig>> pendingRecoveredSigs;

    // must be protected by cs
    FastRandomContext rnd;

    int64_t lastCleanupTime{0};

    std::vector<CRecoveredSigsListener*> recoveredSigsListeners;

public:
    CSigningManager(bool fMemory);

    bool AlreadyHave(const CInv& inv);
    bool GetRecoveredSigForGetData(const uint256& hash, CRecoveredSig& ret);

    void ProcessMessage(CNode* pnode, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);

private:
    void ProcessMessageRecoveredSig(CNode* pfrom, const CRecoveredSig& recoveredSig, CConnman& connman);
    bool PreVerifyRecoveredSig(NodeId nodeId, const CRecoveredSig& recoveredSig, bool& retBan);

    void CollectPendingRecoveredSigsToVerify(size_t maxUniqueSessions, std::map<NodeId, std::list<CRecoveredSig>>& retSigShares, std::map<std::pair<Consensus::LLMQType, uint256>, CQuorumCPtr>& retQuorums);
    void ProcessPendingRecoveredSigs(CConnman& connman); // called from the worker thread of CSigSharesManager
    void ProcessRecoveredSig(NodeId nodeId, const CRecoveredSig& recoveredSig, const CQuorumCPtr& quorum, CConnman& connman);
    void Cleanup(); // called from the worker thread of CSigSharesManager

public:
    // public interface
    void RegisterRecoveredSigsListener(CRecoveredSigsListener* l);
    void UnregisterRecoveredSigsListener(CRecoveredSigsListener* l);
    bool AsyncSignIfMember(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);
    bool HasRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);
    bool HasRecoveredSigForId(Consensus::LLMQType llmqType, const uint256& id);
    bool HasRecoveredSigForSession(const uint256& signHash);
    bool IsConflicting(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);
    CQuorumCPtr SelectQuorumForSigning(Consensus::LLMQType llmqType, int signHeight, const uint256& selectionHash);
    // Verifies a recovered sig that was signed while the chain tip was at signedAtTip
    bool VerifyRecoveredSig(Consensus::LLMQType llmqType, int signedAtHeight, const uint256& id, const uint256& msgHash, const CBLSSignature& sig);
};

extern CSigningManager* quorumSigningManager;

} // namespace llmq

#endif // BLKC_QUORUMS_SIGNING_H