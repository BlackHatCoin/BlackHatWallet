// Copyright (c) 2021-2022 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_TIERTWO_INIT_H
#define BLKC_TIERTWO_INIT_H

#include <string>
#include "fs.h"

static const bool DEFAULT_MASTERNODE  = false;
static const bool DEFAULT_MNCONFLOCK = true;

class CScheduler;
namespace boost {
    class thread_group;
}

std::string GetTierTwoHelpString(bool showDebug);

/** Init the interfaces objects */
void InitTierTwoInterfaces();

/** Resets the interfaces objects */
void ResetTierTwoInterfaces();

/** Inits the tier two global objects */
void InitTierTwoPreChainLoad(bool fReindex);

/** Inits the tier two global objects that require access to the coins tip cache */
void InitTierTwoPostCoinsCacheLoad(CScheduler* scheduler);

/** Initialize chain tip */
void InitTierTwoChainTip();

/** Loads from disk all the tier two related objects */
bool LoadTierTwo(int chain_active_height, bool load_cache_files);

/** Register all tier two objects */
void RegisterTierTwoValidationInterface();

/** Dump tier two managers to disk */
void DumpTierTwo();

void SetBudgetFinMode(const std::string& mode);

/** Initialize the active Masternode manager */
bool InitActiveMN();

/** Starts tier two threads and jobs */
void StartTierTwoThreadsAndScheduleJobs(boost::thread_group& threadGroup, CScheduler& scheduler);

/** Stops tier two workers */
void StopTierTwoThreads();

/** Cleans manager and worker objects pointers */
void DeleteTierTwo();


#endif //BLKC_TIERTWO_INIT_H
