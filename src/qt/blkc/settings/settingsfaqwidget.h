// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSFAQWIDGET_H
#define SETTINGSFAQWIDGET_H

#include <QDialog>

class BLKCGUI;

namespace Ui {
class SettingsFaqWidget;
}

class SettingsFaqWidget : public QDialog
{
    Q_OBJECT
public:
    enum Section {
        INTRO,
        UNSPENDABLE,
        STAKE,
        SUPPORT,
        MASTERNODE,
        MNCONTROLLER
    };

    explicit SettingsFaqWidget(BLKCGUI *parent = nullptr);
    ~SettingsFaqWidget();

    void showEvent(QShowEvent *event) override;

public Q_SLOTS:
   void windowResizeEvent(QResizeEvent* event);
   void setSection(Section _section);
private Q_SLOTS:
    void onFaqClicked(const QWidget* const widget);
private:
    Ui::SettingsFaqWidget *ui;
    Section section = INTRO;

    // This needs to be edited if changes are made to the Section enum.
    std::vector<QPushButton*> getButtons();
};

#endif // SETTINGSFAQWIDGET_H
