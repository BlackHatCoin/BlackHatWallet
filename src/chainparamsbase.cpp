// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2021 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"

#include "tinyformat.h"
#include "util/system.h"

#include <assert.h>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::REGTEST = "regtest";

void AppendParamsHelpMessages(std::string& strUsage, bool debugHelp)
{
    strUsage += HelpMessageGroup("Chain selection options:");
    strUsage += HelpMessageOpt("-testnet", "Use the test chain");
    if (debugHelp) {
        strUsage += HelpMessageOpt("-regtest", "Enter regression test mode, which uses a special chain in which blocks can be solved instantly. "
                                               "This is intended for regression testing tools and app development.");
    }
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::make_unique<CBaseChainParams>("", 7148);
    else if (chain == CBaseChainParams::TESTNET)
        return std::make_unique<CBaseChainParams>("testnet5", 7150);
    else if (chain == CBaseChainParams::REGTEST)
        return std::make_unique<CBaseChainParams>("regtest", 7150);
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectBaseParams(const std::string& chain)
{
    globalChainBaseParams = CreateBaseChainParams(chain);
    gArgs.SelectConfigNetwork(chain);
}
