// Copyright (c) 2020-2021 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "destination_io.h"
#include "key_io.h"
#include "sapling/key_io_sapling.h"

namespace Standard {

    std::string EncodeDestination(const CWDestination &address, const CChainParams::Base58Type addrType) {
        const CTxDestination *dest = boost::get<CTxDestination>(&address);
        if (!dest) {
            return KeyIO::EncodePaymentAddress(*boost::get<libzcash::SaplingPaymentAddress>(&address));
        }
        return EncodeDestination(*dest, addrType);
    };

    CWDestination DecodeDestination(const std::string& strAddress)
    {
        bool isStaking = false;
        bool isExchange = false;
        return DecodeDestination(strAddress, isStaking, isExchange);
    }

    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isExchange)
    {
        bool isShielded = false;
        return DecodeDestination(strAddress, isStaking, isExchange, isShielded);
    }

    // agregar isShielded
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isExchange, bool& isShielded)
    {
        CWDestination dest;
        CTxDestination regDest = ::DecodeDestination(strAddress, isStaking, isExchange);
        if (!IsValidDestination(regDest)) {
            const auto sapDest = KeyIO::DecodeSaplingPaymentAddress(strAddress);
            if (sapDest) {
                isShielded = true;
                return *sapDest;
            }
        }
        return regDest;

    }

    bool IsValidDestination(const CWDestination& address)
    {
        // Only regular base58 addresses and shielded addresses accepted here for now
        const libzcash::SaplingPaymentAddress *dest1 = boost::get<libzcash::SaplingPaymentAddress>(&address);
        if (dest1) return true;

        const CTxDestination *dest = boost::get<CTxDestination>(&address);
        return dest && ::IsValidDestination(*dest);
    }

    const libzcash::SaplingPaymentAddress* GetShieldedDestination(const CWDestination& dest)
    {
        return boost::get<libzcash::SaplingPaymentAddress>(&dest);
    }

    const CTxDestination* GetTransparentDestination(const CWDestination& dest)
    {
        return boost::get<CTxDestination>(&dest);
    }

} // End Standard namespace

Destination& Destination::operator=(const Destination& from)
{
    this->dest = from.dest;
    this->isP2CS = from.isP2CS;
    this->isExchange = from.isExchange;
    return *this;
}

// Returns the key ID if Destination is a transparent "regular" destination
const CKeyID* Destination::getKeyID()
{
    const CTxDestination* regDest = Standard::GetTransparentDestination(dest);
    return (regDest) ? boost::get<CKeyID>(regDest) : nullptr;
}

std::string Destination::ToString() const
{
    if (!Standard::IsValidDestination(dest)) {
        // Invalid address
        return "";
    }
    CChainParams::Base58Type addrType = isP2CS ? CChainParams::STAKING_ADDRESS
                                 : (isExchange ? CChainParams::EXCHANGE_ADDRESS
                                 : CChainParams::PUBKEY_ADDRESS);
    return Standard::EncodeDestination(dest, addrType);
}

