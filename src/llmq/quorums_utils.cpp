// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_utils.h"

#include "bls/bls_wrapper.h"
#include "chainparams.h"
#include "hash.h"
#include "quorums.h"
#include "quorums_utils.h"
#include "random.h"
#include "validation.h"

namespace llmq
{

namespace utils
{

uint256 BuildCommitmentHash(Consensus::LLMQType llmqType, const uint256& blockHash, const std::vector<bool>& validMembers, const CBLSPublicKey& pubKey, const uint256& vvecHash)
{
    CHashWriter hw(SER_NETWORK, 0);
    hw << static_cast<uint8_t>(llmqType);
    hw << blockHash;
    hw << DYNBITSET(validMembers);
    hw << pubKey;
    hw << vvecHash;
    return hw.GetHash();
}

uint256 BuildSignHash(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash)
{
    CHashWriter h(SER_GETHASH, 0);
    h << static_cast<uint8_t>(llmqType);
    h << quorumHash;
    h << id;
    h << msgHash;
    return h.GetHash();
}

std::string ToHexStr(const std::vector<bool>& vBits)
{
    std::vector<uint8_t> vBytes((vBits.size() + 7) / 8);
    for (size_t i = 0; i < vBits.size(); i++) {
        vBytes[i / 8] |= vBits[i] << (i % 8);
    }
    return HexStr(vBytes);
}

bool IsQuorumActive(Consensus::LLMQType llmqType, const uint256& quorumHash)
{

    auto& params = Params().GetConsensus().llmqs.at(llmqType);

    // sig shares and recovered sigs are only accepted from recent/active quorums
    // we allow one more active quorum as specified in consensus, as otherwise there is a small window where things could
    // fail while we are on the brink of a new quorum
    auto quorums = quorumManager->ScanQuorums(llmqType, (int)params.signingActiveQuorumCount + 1);
    for (auto& q : quorums) {
        if (q->pindexQuorum->GetBlockHash() == quorumHash) {
            return true;
        }
    }
    return false;
}

template <typename CacheType>
void InitQuorumsCache(CacheType& cache)
{
    for (auto& llmq : Params().GetConsensus().llmqs) {
        cache.emplace(std::piecewise_construct, std::forward_as_tuple(llmq.first),
            std::forward_as_tuple(llmq.second.signingActiveQuorumCount + 1));
    }
}

template void InitQuorumsCache<std::map<Consensus::LLMQType, unordered_lru_cache<uint256, bool, StaticSaltedHasher>>>(std::map<Consensus::LLMQType, unordered_lru_cache<uint256, bool, StaticSaltedHasher>>& cache);
template void InitQuorumsCache<std::map<Consensus::LLMQType, unordered_lru_cache<uint256, std::vector<CQuorumCPtr>, StaticSaltedHasher>>>(std::map<Consensus::LLMQType, unordered_lru_cache<uint256, std::vector<CQuorumCPtr>, StaticSaltedHasher>>& cache);
template void InitQuorumsCache<std::map<Consensus::LLMQType, unordered_lru_cache<uint256, CQuorumCPtr, StaticSaltedHasher>>>(std::map<Consensus::LLMQType, unordered_lru_cache<uint256, CQuorumCPtr, StaticSaltedHasher>>& cache);

} // namespace llmq::utils

} // namespace llmq
