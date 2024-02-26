// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "chain.h"
#include "chainparams.h"
#include "reverse_iterate.h"

#include <stdint.h>


namespace Checkpoints
{
/**
     * How many times we expect transactions after the last checkpoint to
     * be slower. This number is a compromise, as it can't be accurate for
     * every system. When reindexing from a fast disk with a slow CPU, it
     * can be up to 20, while when downloading from a slow network with a
     * fast multicore CPU, it won't be much higher than 1.
     */
static const double SIGCHECK_VERIFICATION_FACTOR = 5.0;

bool fEnabled = true;

bool CheckBlock(int nHeight, const uint256& hash, bool fMatchesCheckpoint)
{
    if (!fEnabled)
        return true;

    const MapCheckpoints& checkpoints = *Params().Checkpoints().mapCheckpoints;

    MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
    // If looking for an exact match, then return false
    if (i == checkpoints.end()) return !fMatchesCheckpoint;
    return hash == i->second;
}

//! Guess how far we are in the verification process at the given block index
double GuessVerificationProgress(const CBlockIndex* pindex, bool fSigchecks)
{
    if (pindex == nullptr)
        return 0.0;

    int64_t nNow = time(nullptr);

    double fSigcheckVerificationFactor = fSigchecks ? SIGCHECK_VERIFICATION_FACTOR : 1.0;
    double fWorkBefore = 0.0; // Amount of work done before pindex
    double fWorkAfter = 0.0;  // Amount of work left after pindex (estimated)
    // Work is defined as: 1.0 per transaction before the last checkpoint, and
    // fSigcheckVerificationFactor per transaction after.

    const CCheckpointData& data = Params().Checkpoints();

    if (pindex->nChainTx <= data.nTransactionsLastCheckpoint) {
        double nCheapBefore = pindex->nChainTx;
        double nCheapAfter = data.nTransactionsLastCheckpoint - pindex->nChainTx;
        double nExpensiveAfter = (nNow - data.nTimeLastCheckpoint) / 86400.0 * data.fTransactionsPerDay;
        fWorkBefore = nCheapBefore;
        fWorkAfter = nCheapAfter + nExpensiveAfter * fSigcheckVerificationFactor;
    } else {
        double nCheapBefore = data.nTransactionsLastCheckpoint;
        double nExpensiveBefore = pindex->nChainTx - data.nTransactionsLastCheckpoint;
        double nExpensiveAfter = (nNow - pindex->GetBlockTime()) / 86400.0 * data.fTransactionsPerDay;
        fWorkBefore = nCheapBefore + nExpensiveBefore * fSigcheckVerificationFactor;
        fWorkAfter = nExpensiveAfter * fSigcheckVerificationFactor;
    }

    return fWorkBefore / (fWorkBefore + fWorkAfter);
}

int GetTotalBlocksEstimate()
{
    if (!fEnabled)
        return 0;

    const MapCheckpoints& checkpoints = *Params().Checkpoints().mapCheckpoints;

    return checkpoints.rbegin()->first;
}

} // namespace Checkpoints
