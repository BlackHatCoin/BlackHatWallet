// Copyright (c) 2019-2020 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCHANGEADDRESSDIALOG_H
#define SENDCHANGEADDRESSDIALOG_H

#include "destination_io.h"
#include "qt/blkc/focuseddialog.h"
#include "qt/blkc/snackbar.h"
#include "script/standard.h"

class WalletModel;

namespace Ui {
class SendChangeAddressDialog;
}

class SendChangeAddressDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit SendChangeAddressDialog(QWidget* parent, WalletModel* model, bool isTransparent);
    ~SendChangeAddressDialog();

    void setAddress(QString address);
    CWDestination getDestination() const;

    void showEvent(QShowEvent* event) override;

private:
    bool isTransparent;
    WalletModel* walletModel;
    Ui::SendChangeAddressDialog *ui;
    SnackBar *snackBar = nullptr;
    CWDestination dest;

    void inform(const QString& text);

private Q_SLOTS:
    void reset();
    void accept() override;
};

#endif // SENDCHANGEADDRESSDIALOG_H
