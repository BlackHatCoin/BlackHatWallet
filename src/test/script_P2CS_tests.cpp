// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "test/test_blkc.h"

#include "base58.h"
#include "key.h"
#include "policy/policy.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(script_P2CS_tests, TestingSetup)

void CheckValidKeyId(const CTxDestination& dest, const CKeyID& expectedKey)
{
    const CKeyID* keyid = boost::get<CKeyID>(&dest);
    if (keyid) {
        BOOST_CHECK(keyid);
        BOOST_CHECK(*keyid == expectedKey);
    } else {
        BOOST_ERROR("Destination is not a CKeyID");
    }
}

// Goal: check cold staking script keys extraction
BOOST_AUTO_TEST_CASE(extract_cold_staking_destination_keys)
{
    CKey ownerKey;
    ownerKey.MakeNewKey(true);
    CKeyID ownerId = ownerKey.GetPubKey().GetID();
    CKey stakerKey;
    stakerKey.MakeNewKey(true);
    CKeyID stakerId = stakerKey.GetPubKey().GetID();
    CScript script = GetScriptForStakeDelegation(stakerId, ownerId);

    // Check owner
    CTxDestination ownerDest;
    BOOST_CHECK(ExtractDestination(script, ownerDest, false));
    CheckValidKeyId(ownerDest, ownerId);

    // Check staker
    CTxDestination stakerDest;
    BOOST_CHECK(ExtractDestination(script, stakerDest, true));
    CheckValidKeyId(stakerDest, stakerId);

    // Now go with ExtractDestinations.
    txnouttype type;
    int nRequiredRet = -1;
    std::vector<CTxDestination> destVector;
    BOOST_CHECK(ExtractDestinations(script, type, destVector, nRequiredRet));
    BOOST_CHECK(type == TX_COLDSTAKE);
    BOOST_CHECK(nRequiredRet == 2);
    BOOST_CHECK(destVector.size() == 2);
    CheckValidKeyId(destVector[0], stakerId);
    CheckValidKeyId(destVector[1], ownerId);
}

static CScript GetNewP2CS(CKey& stakerKey, CKey& ownerKey)
{
    stakerKey = DecodeSecret("91yo52JPHDVUG3jXWLKGyzEdjn1a9nbnurLdmQEf2UzbgzkTc2c");
    ownerKey = DecodeSecret("92KgNFNfmVVJRQuzssETc7NhwufGuHsLvPQxW9Nwmxs7PB4ByWB");
    return GetScriptForStakeDelegation(stakerKey.GetPubKey().GetID(),
                                       ownerKey.GetPubKey().GetID());
}

static CScript GetDummyP2CS(const CKeyID& dummyKeyID)
{
    return GetScriptForStakeDelegation(dummyKeyID, dummyKeyID);
}

static CScript GetDummyP2PKH(const CKeyID& dummyKeyID)
{
    return GetScriptForDestination(dummyKeyID);
}

static const CAmount amtIn = 200 * COIN;
static const unsigned int flags = STANDARD_SCRIPT_VERIFY_FLAGS;

static CMutableTransaction CreateNewColdStakeTx(CScript& scriptP2CS, CKey& stakerKey, CKey& ownerKey)
{
    scriptP2CS = GetNewP2CS(stakerKey, ownerKey);

    // Create prev transaction:
    CMutableTransaction txFrom;
    txFrom.vout.resize(1);
    txFrom.vout[0].nValue = amtIn;
    txFrom.vout[0].scriptPubKey = scriptP2CS;

    // Create coldstake
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vout.resize(2);
    tx.vin[0].prevout.n = 0;
    tx.vin[0].prevout.hash = txFrom.GetHash();
    tx.vout[0].nValue = 0;
    tx.vout[0].scriptPubKey.clear();
    tx.vout[1].nValue = amtIn + 2 * COIN;
    tx.vout[1].scriptPubKey = scriptP2CS;

    return tx;
}

void SignColdStake(CMutableTransaction& tx, int nIn, const CScript& prevScript, const CKey& key, bool fStaker)
{
    assert(nIn < (int) tx.vin.size());
    tx.vin[nIn].scriptSig.clear();
    const CTransaction _tx(tx);
    SigVersion sv = _tx.GetRequiredSigVersion();
    const uint256& hash = SignatureHash(prevScript, _tx, nIn, SIGHASH_ALL, amtIn, sv);
    std::vector<unsigned char> vchSig;
    BOOST_CHECK(key.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    std::vector<unsigned char> selector(1, fStaker ? (int) OP_TRUE : OP_FALSE);
    tx.vin[nIn].scriptSig << vchSig << selector << ToByteVector(key.GetPubKey());
}

static bool CheckP2CSScript(const CScript& scriptSig, const CScript& scriptPubKey, const CMutableTransaction& tx, ScriptError& err)
{
    err = SCRIPT_ERR_OK;
    return VerifyScript(scriptSig, scriptPubKey, flags, MutableTransactionSignatureChecker(&tx, 0, amtIn), tx.GetRequiredSigVersion(), &err);
}

BOOST_AUTO_TEST_CASE(coldstake_script)
{
    SelectParams(CBaseChainParams::REGTEST);
    CScript scriptP2CS;
    CKey stakerKey, ownerKey;

    // create unsigned coinstake transaction
    CMutableTransaction good_tx = CreateNewColdStakeTx(scriptP2CS, stakerKey, ownerKey);

    // sign the input with the staker key
    SignColdStake(good_tx, 0, scriptP2CS, stakerKey, true);

    // check the signature and script
    ScriptError err = SCRIPT_ERR_OK;
    CMutableTransaction tx(good_tx);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));

    // pay less than expected
    tx.vout[1].nValue -= 3 * COIN;
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(!CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_CHECKCOLDSTAKEVERIFY, ScriptErrorString(err));

    // Add another p2cs out
    tx.vout.emplace_back(3 * COIN, scriptP2CS);
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));

    const CKey& dummyKey = DecodeSecret("91t7cwPGevo885Uccg87nVjzUxKhXta9JprHM3R21PQkBFMFg2i");
    const CKeyID& dummyKeyID = dummyKey.GetPubKey().GetID();
    const CScript& dummyP2PKH = GetDummyP2PKH(dummyKeyID);

    // Add a masternode out
    tx.vout.emplace_back(3 * COIN, dummyP2PKH);
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));

    // Transfer more coins to the masternode
    tx.vout[2].nValue -= 2 * COIN;
    tx.vout[3].nValue += 2 * COIN;
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(!CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_CHECKCOLDSTAKEVERIFY, ScriptErrorString(err));

    // Add two "free" outputs
    tx = good_tx;
    tx.vout[1].nValue -= 3 * COIN;
    tx.vout.emplace_back(3 * COIN, dummyP2PKH);
    tx.vout.emplace_back(3 * COIN, dummyP2PKH);
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(!CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_CHECKCOLDSTAKEVERIFY, ScriptErrorString(err));
    // -- but the owner can
    SignColdStake(tx, 0, scriptP2CS, ownerKey, false);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));

    // Replace with new p2cs
    tx = good_tx;
    tx.vout[1].scriptPubKey = GetDummyP2CS(dummyKeyID);
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(!CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_CHECKCOLDSTAKEVERIFY, ScriptErrorString(err));
    // -- but the owner can
    SignColdStake(tx, 0, scriptP2CS, ownerKey, false);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));

    // Replace with single dummy out
    tx = good_tx;
    tx.vout[1] = CTxOut(COIN, dummyP2PKH);
    SignColdStake(tx, 0, scriptP2CS, stakerKey, true);
    BOOST_CHECK(!CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_CHECKCOLDSTAKEVERIFY, ScriptErrorString(err));
    // -- but the owner can
    SignColdStake(tx, 0, scriptP2CS, ownerKey, false);
    BOOST_CHECK(CheckP2CSScript(tx.vin[0].scriptSig, scriptP2CS, tx, err));
}

BOOST_AUTO_TEST_SUITE_END()
