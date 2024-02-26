// Copyright (c) 2019 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSSIGNMESSAGEWIDGETS_H
#define SETTINGSSIGNMESSAGEWIDGETS_H

#include <QWidget>
#include "qt/blkc/pwidget.h"
#include "qt/blkc/contactsdropdown.h"

namespace Ui {
class SettingsSignMessageWidgets;
}

class SettingsSignMessageWidgets : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsSignMessageWidgets(BLKCGUI* _window, QWidget *parent = nullptr);
    ~SettingsSignMessageWidgets();

    void setAddress_SM(const QString& address);
    void showEvent(QShowEvent *event) override;
protected:
    void resizeEvent(QResizeEvent *event) override;
public Q_SLOTS:
    void onSignMessageButtonSMClicked();
    void onVerifyMessage();
    void onGoClicked();
    void onClearAll();
    void onAddressesClicked();
    void onModeSelected(bool isSign);
    void updateMode();
private:
    Ui::SettingsSignMessageWidgets *ui;
    QAction *btnContact;
    ContactsDropdown *menuContacts = nullptr;
    bool isSign = true;
    void resizeMenu();
};

#endif // SETTINGSSIGNMESSAGEWIDGETS_H
