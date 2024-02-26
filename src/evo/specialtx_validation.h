    // Copyright (c) 2017 The Dash Core developers
// Copyright (c) 2020-2022 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_SPECIALTX_H
#define BLKC_SPECIALTX_H

#include "llmq/quorums_commitment.h"
#include "validation.h" // cs_main needed by CheckLLMQCommitment (!TODO: remove)
#include "version.h"

class CBlock;
class CBlockIndex;
class CCoinsViewCache;
class CValidationState;
class CTransaction;
class uint256;

/** The maximum allowed size of the extraPayload (for any TxType) */
static const unsigned int MAX_SPECIALTX_EXTRAPAYLOAD = 10000;

/** Payload validity checks (including duplicate unique properties against list at pindexPrev)*/
// Note: for +v2, if the tx is not a special tx, this method returns true.
// Note2: This function only performs extra payload related checks, it does NOT checks regular inputs and outputs.
bool CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, const CCoinsViewCache* view, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

// Basic non-contextual checks for special txes
// Note: for +v2, if the tx is not a special tx, this method returns true.
bool CheckSpecialTxNoContext(const CTransaction& tx, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

// Update internal tiertwo data when blocks containing special txes get connected/disconnected
bool ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, const CCoinsViewCache* view, CValidationState& state, bool fJustCheck) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex);

// Validate given LLMQ final commitment with the list at pindexQuorum
bool VerifyLLMQCommitment(const llmq::CFinalCommitment& qfc, const CBlockIndex* pindexPrev, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

uint256 CalcTxInputsHash(const CTransaction& tx);

#endif // BLKC_SPECIALTX_H
