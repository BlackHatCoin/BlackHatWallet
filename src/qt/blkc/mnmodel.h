// Copyright (c) 2019-2020 The PIVX developers
// Copyright (c) 2021 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNMODEL_H
#define MNMODEL_H

#include <QAbstractTableModel>
#include "masternodeconfig.h"
#include "qt/walletmodel.h"

class CMasternode;

class MNModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MNModel(QObject *parent);
    ~MNModel() override {
        nodes.clear();
        collateralTxAccepted.clear();
    }
    void init();
    void setWalletModel(WalletModel* _model) { walletModel = _model; };

    enum ColumnIndex {
        ALIAS = 0,  /**< User specified MN alias */
        ADDRESS = 1, /**< Node address */
        PROTO_VERSION = 2, /**< Node protocol version */
        STATUS = 3, /**< Node status */
        ACTIVE_TIMESTAMP = 4, /**<  */
        PUB_KEY = 5,
        COLLATERAL_ID = 6,
        COLLATERAL_OUT_INDEX = 7,
        PRIV_KEY = 8,
        WAS_COLLATERAL_ACCEPTED = 9
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    bool removeMn(const QModelIndex& index);
    bool addMn(CMasternodeConfig::CMasternodeEntry* entry);
    void updateMNList();


    bool isMNsNetworkSynced();
    // Returns the MN activeState field.
    int getMNState(const QString& mnAlias);
    // Checks if the masternode is inactive
    bool isMNInactive(const QString& mnAlias);
    // Masternode is active if it's in PRE_ENABLED OR ENABLED state
    bool isMNActive(const QString& mnAlias);
    // Masternode collateral has enough confirmations
    bool isMNCollateralMature(const QString& mnAlias);
    // Validate string representing a masternode IP address
    static bool validateMNIP(const QString& addrStr);

    // Return the specific chain amount value for the MN collateral output.
    CAmount getMNCollateralRequiredAmount();
    // Return the specific chain min conf for the collateral tx
    int getMasternodeCollateralMinConf();
    // Generates the collateral transaction
    bool createMNCollateral(const QString& alias, const QString& addr, COutPoint& ret_outpoint, QString& ret_error);
    // Creates the mnb and broadcast it to the network
    bool startLegacyMN(const CMasternodeConfig::CMasternodeEntry& mne, int chainHeight, std::string& strError);
    void startAllLegacyMNs(bool onlyMissing, int& amountOfMnFailed, int& amountOfMnStarted,
                           std::string* aliasFilter = nullptr, std::string* error_ret = nullptr);

    CMasternodeConfig::CMasternodeEntry* createLegacyMN(COutPoint& collateralOut,
                                                        const std::string& alias,
                                                        std::string& serviceAddr,
                                                        const std::string& port,
                                                        const std::string& mnKeyString,
                                                        QString& ret_error);

    bool removeLegacyMN(const std::string& alias_to_remove, const std::string& tx_id, unsigned int out_index, QString& ret_error);

private:
    WalletModel* walletModel;
    // alias mn node ---> pair <ip, master node>
    QMap<QString, std::pair<QString, CMasternode*>> nodes;
    QMap<std::string, bool> collateralTxAccepted;
};

#endif // MNMODEL_H
