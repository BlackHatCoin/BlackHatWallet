// Copyright (c) 2020 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BLKC_BALANCEBUBBLE_H
#define BLKC_BALANCEBUBBLE_H

#include <QWidget>
#include <QString>

namespace Ui {
    class BalanceBubble;
}

class BalanceBubble : public QWidget
{

public:
    explicit BalanceBubble(QWidget *parent = nullptr);
    ~BalanceBubble();

    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

    void updateValues(int64_t nTransparentBalance, int64_t nShieldedBalance, int unit);

public Q_SLOTS:
    void hideTimeout();

private:
    Ui::BalanceBubble *ui;
    QTimer* hideTimer{nullptr};
};

#endif //BLKC_BALANCEBUBBLE_H
