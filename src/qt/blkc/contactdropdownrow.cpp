// Copyright (c) 2019 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/blkc/contactdropdownrow.h"
#include "qt/blkc/forms/ui_contactdropdownrow.h"

ContactDropdownRow::ContactDropdownRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ContactDropdownRow)
{
    ui->setupUi(this);
    ui->lblAddress->setProperty("cssClass", "text-list-contact-body1");
    ui->lblLabel->setProperty("cssClass", "text-list-contact-title1");
}

void ContactDropdownRow::init(bool isLightTheme, bool isHover) {
    update(isLightTheme, isHover, false);
}

void ContactDropdownRow::update(bool isLightTheme, bool isHover, bool isSelected){
    ui->lblDivisory->setStyleSheet("background-color:#bababa");
}

void ContactDropdownRow::setData(QString address, QString label){
    ui->lblAddress->setText(address);
    ui->lblLabel->setText(label);
}

ContactDropdownRow::~ContactDropdownRow()
{
    delete ui;
}
