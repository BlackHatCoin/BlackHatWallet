// Copyright (c) 2019-2022 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODEWIZARDDIALOG_H
#define MASTERNODEWIZARDDIALOG_H

#include "qt/blkc/focuseddialog.h"
#include "qt/blkc/snackbar.h"
#include "masternodeconfig.h"
#include "qt/blkc/pwidget.h"

class MNModel;
class WalletModel;

namespace Ui {
class MasterNodeWizardDialog;
class QPushButton;
}

class MasterNodeWizardDialog : public FocusedDialog, public PWidget::Translator
{
    Q_OBJECT

public:
    explicit MasterNodeWizardDialog(WalletModel* walletMode,
                                    MNModel* mnModel,
                                    QWidget *parent = nullptr);
    ~MasterNodeWizardDialog() override;
    void showEvent(QShowEvent *event) override;
    QString translate(const char *msg) override { return tr(msg); }

    QString returnStr = "";
    bool isOk = false;
    CMasternodeConfig::CMasternodeEntry* mnEntry = nullptr;

private Q_SLOTS:
    void accept() override;
    void onBackClicked();
private:
    Ui::MasterNodeWizardDialog *ui;
    QPushButton* icConfirm1;
    QPushButton* icConfirm3;
    QPushButton* icConfirm4;
    SnackBar *snackBar = nullptr;
    int pos = 0;

    WalletModel* walletModel{nullptr};
    MNModel* mnModel{nullptr};
    bool createMN();
    void inform(const QString& text);
};

#endif // MASTERNODEWIZARDDIALOG_H
