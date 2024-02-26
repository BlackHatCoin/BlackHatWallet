// Copyright (c) 2021 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_BLOCKSTATECATCHER_H
#define BLKC_BLOCKSTATECATCHER_H

#include "consensus/validation.h"
#include "validationinterface.h"

/**
 * Validation interface listener used to get feedback from ProcessNewBlock result.
 */
class BlockStateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;
    bool isRegistered{false};

    BlockStateCatcher(const uint256& hashIn) : hash(hashIn), found(false), state(){};
    ~BlockStateCatcher() { if (isRegistered) UnregisterValidationInterface(this); }
    void registerEvent() { RegisterValidationInterface(this); isRegistered = true; }
    void setBlockHash(const uint256& _hash) { clear(); hash = _hash; }
    void clear() { hash.SetNull(); found = false; state = CValidationState(); }
    bool stateErrorFound() { return found && state.IsError(); }

protected:
    virtual void BlockChecked(const CBlock& block, const CValidationState& stateIn) {
        if (block.GetHash() != hash) return;
        found = true;
        state = stateIn;
    };
};

#endif //BLKC_BLOCKSTATECATCHER_H
