// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2021 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODE_SYNC_H
#define MASTERNODE_SYNC_H

#include "net.h"    // for NodeId
#include "uint256.h"

#include <atomic>
#include <string>
#include <map>

#define MASTERNODE_SYNC_TIMEOUT 5

class CMasternodeSync;
extern CMasternodeSync masternodeSync;

struct TierTwoPeerData {
    // map of message --> last request timestamp, bool hasResponseArrived.
    std::map<const char*, std::pair<int64_t, bool>> mapMsgData;
};

//
// CMasternodeSync : Sync masternode assets in stages
//

class CMasternodeSync
{
public:
    int64_t lastFailure;
    int nCountFailures;

    std::atomic<int64_t> lastProcess;

    // sum of all counts
    int sumMasternodeList;
    int sumMasternodeWinner;
    int sumBudgetItemProp;
    int sumBudgetItemFin;
    // peers that reported counts
    int countMasternodeList;
    int countMasternodeWinner;
    int countBudgetItemProp;
    int countBudgetItemFin;

    // Count peers we've requested the list from
    int RequestedMasternodeAttempt;

    // Time when current masternode asset sync started
    int64_t nAssetSyncStarted;

    CMasternodeSync();

    void SwitchToNextAsset();
    std::string GetSyncStatus();
    void ProcessSyncStatusMsg(int nItemID, int itemCount);
    bool IsBudgetFinEmpty();
    bool IsBudgetPropEmpty();

    void Reset();
    void Process();
    /*
     * Process sync with a single node.
     * If it returns false, the Process() step is complete.
     * Otherwise Process() calls it again for a different node.
     */
    bool SyncWithNode(CNode* pnode, bool fLegacyMnObsolete);
    bool NotCompleted();
    void UpdateBlockchainSynced(bool isRegTestNet);
    void ClearFulfilledRequest();

    // Sync message dispatcher
    bool MessageDispatcher(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

private:

    // Tier two sync node state
    // map of nodeID --> TierTwoPeerData
    std::map<NodeId, TierTwoPeerData> peersSyncState;
    static int GetNextAsset(int currentAsset);

    void SyncRegtest(CNode* pnode);

    template <typename... Args>
    void RequestDataTo(CNode* pnode, const char* msg, bool forceRequest, Args&&... args);

    template <typename... Args>
    void PushMessage(CNode* pnode, const char* msg, Args&&... args);

    // update peer sync state data
    bool UpdatePeerSyncState(const NodeId& id, const char* msg, const int nextSyncStatus);

    // Check if an update is needed
    void CheckAndUpdateSyncStatus();

    // Mark sync timeout
    void syncTimeout(const std::string& reason);
};

#endif
