// Copyright (c) 2018-2019 The Dash Core developers
// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_QUORUMS_COMMITMENT_H
#define BLKC_QUORUMS_COMMITMENT_H

#include "bls/bls_wrapper.h"
#include "consensus/params.h"

#include <univalue.h>

class CBlockIndex;
class CValidationState;
class CTransaction;

namespace llmq
{

// This message is an aggregation of all received premature commitments and only valid if
// enough (>=threshold) premature commitments were aggregated
// This is mined on-chain as part of LLMQCOMM payload.
class CFinalCommitment
{
public:
    static const uint16_t CURRENT_VERSION = 1;

    uint16_t nVersion{CURRENT_VERSION};
    uint8_t llmqType{Consensus::LLMQ_NONE};
    uint256 quorumHash;
    std::vector<bool> signers;
    std::vector<bool> validMembers;

    CBLSPublicKey quorumPublicKey;
    uint256 quorumVvecHash;

    CBLSSignature quorumSig; // recovered threshold sig of blockHash+validMembers+pubKeyHash+vvecHash
    CBLSSignature membersSig; // aggregated member sig of blockHash+validMembers+pubKeyHash+vvecHash

public:
    CFinalCommitment() = default;
    CFinalCommitment(const Consensus::LLMQParams& params, const uint256& _quorumHash);

    int CountSigners() const { return (int)std::count(signers.begin(), signers.end(), true); }
    int CountValidMembers() const { return (int)std::count(validMembers.begin(), validMembers.end(), true); }

    bool IsNull() const;
    void ToJson(UniValue& obj) const;

    bool Verify(const std::vector<CBLSPublicKey>& allkeys, const Consensus::LLMQParams& params) const;
    bool VerifySizes(const Consensus::LLMQParams& params) const;

    SERIALIZE_METHODS(CFinalCommitment, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.llmqType);
        READWRITE(obj.quorumHash);
        READWRITE(DYNBITSET(obj.signers));
        READWRITE(DYNBITSET(obj.validMembers));
        READWRITE(obj.quorumPublicKey);
        READWRITE(obj.quorumVvecHash);
        READWRITE(obj.quorumSig);
        READWRITE(obj.membersSig);
    }
};

class LLMQCommPL
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};
    uint32_t nHeight{(uint32_t)-1};
    CFinalCommitment commitment;

public:
    SERIALIZE_METHODS(LLMQCommPL, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.nHeight);
        READWRITE(obj.commitment);
    }

    void ToJson(UniValue& obj) const;
};

} // namespace llmq

#endif // BLKC_QUORUMS_COMMITMENT_H
