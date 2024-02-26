// Copyright (c) 2021-2022 The PIVX Core developers
// Copyright (c) 2021-2024 The BlackHat developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_blkc.h"

#include "consensus/validation.h"
#include "evo/providertx.h"
#include "evo/specialtx_validation.h"
#include "llmq/quorums_commitment.h"
#include "messagesigner.h"
#include "netbase.h"
#include "primitives/transaction.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(evo_specialtx_tests, TestingSetup)

static CKey GetRandomKey()
{
    CKey key;
    key.MakeNewKey(true);
    return key;
}

static CKeyID GetRandomKeyID()
{
    return GetRandomKey().GetPubKey().GetID();
}

static CBLSPublicKey GetRandomBLSKey()
{
    CBLSSecretKey sk;
    sk.MakeNewKey();
    return sk.GetPublicKey();
}

static CScript GetRandomScript()
{
    return GetScriptForDestination(GetRandomKeyID());
}

static ProRegPL GetRandomProRegPayload()
{
    ProRegPL pl;
    pl.collateralOutpoint.hash = GetRandHash();
    pl.collateralOutpoint.n = InsecureRandBits(2);
    BOOST_CHECK(Lookup("57.12.210.11:7147", pl.addr, Params().GetDefaultPort(), false));
    pl.keyIDOwner = GetRandomKeyID();
    pl.pubKeyOperator = GetRandomBLSKey();
    pl.keyIDVoting = GetRandomKeyID();
    pl.scriptPayout = GetRandomScript();
    pl.nOperatorReward = InsecureRandRange(10000);
    pl.scriptOperatorPayout = GetRandomScript();
    pl.inputsHash = GetRandHash();
    pl.vchSig = InsecureRandBytes(63);
    return pl;
}

static ProUpServPL GetRandomProUpServPayload()
{
    ProUpServPL pl;
    pl.proTxHash = GetRandHash();
    BOOST_CHECK(Lookup("127.0.0.1:7147", pl.addr, Params().GetDefaultPort(), false));
    pl.scriptOperatorPayout = GetRandomScript();
    pl.inputsHash = GetRandHash();
    pl.sig.SetByteVector(InsecureRandBytes(BLS_CURVE_SIG_SIZE));
    return pl;
}

static ProUpRegPL GetRandomProUpRegPayload()
{
    ProUpRegPL pl;
    pl.proTxHash = GetRandHash();
    pl.pubKeyOperator = GetRandomBLSKey();
    pl.keyIDVoting = GetRandomKeyID();
    pl.scriptPayout = GetRandomScript();
    pl.inputsHash = GetRandHash();
    pl.vchSig = InsecureRandBytes(63);
    return pl;
}

static ProUpRevPL GetRandomProUpRevPayload()
{
    ProUpRevPL pl;
    pl.proTxHash = GetRandHash();
    pl.nReason = InsecureRand16();
    pl.inputsHash = GetRandHash();
    pl.sig.SetByteVector(InsecureRandBytes(BLS_CURVE_SIG_SIZE));
    return pl;
}

llmq::CFinalCommitment GetRandomLLMQCommitment()
{
    llmq::CFinalCommitment fc;
    fc.nVersion = InsecureRand16();
    fc.llmqType = InsecureRandBits(8);
    fc.quorumHash = GetRandHash();
    int vecsize = InsecureRandRange(500);
    for (int i = 0; i < vecsize; i++) {
        fc.signers.emplace_back((bool)InsecureRandBits(1));
        fc.validMembers.emplace_back((bool)InsecureRandBits(1));
    }
    fc.quorumPublicKey.SetByteVector(InsecureRandBytes(BLS_CURVE_PUBKEY_SIZE));
    fc.quorumVvecHash = GetRandHash();
    fc.quorumSig.SetByteVector(InsecureRandBytes(BLS_CURVE_SIG_SIZE));
    fc.membersSig.SetByteVector(InsecureRandBytes(BLS_CURVE_SIG_SIZE));
    return fc;
}

static llmq::LLMQCommPL GetRandomLLMQCommPayload()
{
    llmq::LLMQCommPL pl;
    pl.nHeight = InsecureRand32();
    pl.commitment = GetRandomLLMQCommitment();
    return pl;
}

static bool EqualCommitments(const llmq::CFinalCommitment& a, const llmq::CFinalCommitment& b)
{
    return a.nVersion == b.nVersion &&
           a.llmqType == b.llmqType &&
           a.quorumHash == b.quorumHash &&
           a.signers == b.signers &&
           a.quorumPublicKey == b.quorumPublicKey &&
           a.quorumVvecHash == b.quorumVvecHash &&
           a.quorumSig == b.quorumSig &&
           a.membersSig == b.membersSig;
}

BOOST_AUTO_TEST_CASE(protx_validation_test)
{
    LOCK(cs_main);

    CMutableTransaction mtx;
    CValidationState state;

    // v1 can only be Type=0
    mtx.nType = CTransaction::TxType::PROREG;
    mtx.nVersion = CTransaction::TxVersion::LEGACY;
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-type-version");

    // version >= Sapling, type = 0, payload != null.
    mtx.nType = CTransaction::TxType::NORMAL;
    mtx.extraPayload = std::vector<uint8_t>(10, 1);
    mtx.nVersion = CTransaction::TxVersion::SAPLING;
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-type-payload");

    // version >= Sapling, type = 0, payload == null --> pass
    mtx.extraPayload = nullopt;
    BOOST_CHECK(CheckSpecialTxNoContext(CTransaction(mtx), state));

    // nVersion>=2 and nType!=0 without extrapayload
    mtx.nType = CTransaction::TxType::PROREG;
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK(state.GetRejectReason().find("without extra payload"));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-payload-empty");

    // Size limits
    mtx.extraPayload = std::vector<uint8_t>(MAX_SPECIALTX_EXTRAPAYLOAD + 1, 1);
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-payload-oversize");

    // Remove one element, so now it passes the size check
    mtx.extraPayload->pop_back();
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-payload");

    // valid payload but invalid inputs hash
    mtx.extraPayload->clear();
    ProRegPL pl = GetRandomProRegPayload();
    SetTxPayload(mtx, pl);
    BOOST_CHECK(!CheckSpecialTxNoContext(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-protx-inputs-hash");

    // all good.
    mtx.vin.emplace_back(GetRandHash(), 0);
    mtx.extraPayload->clear();
    pl.inputsHash = CalcTxInputsHash(CTransaction(mtx));
    SetTxPayload(mtx, pl);
    BOOST_CHECK(CheckSpecialTxNoContext(CTransaction(mtx), state));
}

BOOST_AUTO_TEST_CASE(proreg_setpayload_test)
{
    const ProRegPL& pl = GetRandomProRegPayload();

    CMutableTransaction mtx;
    SetTxPayload(mtx, pl);
    ProRegPL pl2;
    BOOST_CHECK(GetTxPayload(mtx, pl2));
    BOOST_CHECK(pl.collateralOutpoint == pl2.collateralOutpoint);
    BOOST_CHECK(pl.addr  == pl2.addr);
    BOOST_CHECK(pl.keyIDOwner == pl2.keyIDOwner);
    BOOST_CHECK(pl.pubKeyOperator == pl2.pubKeyOperator);
    BOOST_CHECK(pl.keyIDVoting == pl2.keyIDVoting);
    BOOST_CHECK(pl.scriptPayout == pl2.scriptPayout);
    BOOST_CHECK(pl.nOperatorReward  == pl2.nOperatorReward);
    BOOST_CHECK(pl.scriptOperatorPayout == pl2.scriptOperatorPayout);
    BOOST_CHECK(pl.inputsHash == pl2.inputsHash);
    BOOST_CHECK(pl.vchSig == pl2.vchSig);
}

BOOST_AUTO_TEST_CASE(proupserv_setpayload_test)
{
    const ProUpServPL& pl = GetRandomProUpServPayload();

    CMutableTransaction mtx;
    SetTxPayload(mtx, pl);
    ProUpServPL pl2;
    BOOST_CHECK(GetTxPayload(mtx, pl2));
    BOOST_CHECK(pl.proTxHash == pl2.proTxHash);
    BOOST_CHECK(pl.addr  == pl2.addr);
    BOOST_CHECK(pl.scriptOperatorPayout == pl2.scriptOperatorPayout);
    BOOST_CHECK(pl.inputsHash == pl2.inputsHash);
    BOOST_CHECK(pl.sig == pl2.sig);
}

BOOST_AUTO_TEST_CASE(proupreg_setpayload_test)
{
    const ProUpRegPL& pl = GetRandomProUpRegPayload();

    CMutableTransaction mtx;
    SetTxPayload(mtx, pl);
    ProUpRegPL pl2;
    BOOST_CHECK(GetTxPayload(mtx, pl2));
    BOOST_CHECK(pl.proTxHash == pl2.proTxHash);
    BOOST_CHECK(pl.pubKeyOperator == pl2.pubKeyOperator);
    BOOST_CHECK(pl.keyIDVoting == pl2.keyIDVoting);
    BOOST_CHECK(pl.scriptPayout == pl2.scriptPayout);
    BOOST_CHECK(pl.inputsHash == pl2.inputsHash);
    BOOST_CHECK(pl.vchSig == pl2.vchSig);
}

BOOST_AUTO_TEST_CASE(prouprev_setpayload_test)
{
    const ProUpRevPL& pl = GetRandomProUpRevPayload();

    CMutableTransaction mtx;
    SetTxPayload(mtx, pl);
    ProUpRevPL pl2;
    BOOST_CHECK(GetTxPayload(mtx, pl2));
    BOOST_CHECK(pl.proTxHash == pl2.proTxHash);
    BOOST_CHECK(pl.nReason == pl2.nReason);
    BOOST_CHECK(pl.inputsHash == pl2.inputsHash);
    BOOST_CHECK(pl.sig == pl2.sig);
}

BOOST_AUTO_TEST_CASE(proreg_checkstringsig_test)
{
    ProRegPL pl = GetRandomProRegPayload();
    pl.vchSig.clear();
    const CKey& key = GetRandomKey();
    BOOST_CHECK(CMessageSigner::SignMessage(pl.MakeSignString(), pl.vchSig, key));

    std::string strError;
    const CKeyID& keyID = key.GetPubKey().GetID();
    BOOST_CHECK(CMessageSigner::VerifyMessage(keyID, pl.vchSig, pl.MakeSignString(), strError));
    // Change owner address or script payout
    pl.keyIDOwner = GetRandomKeyID();
    BOOST_CHECK(!CMessageSigner::VerifyMessage(keyID, pl.vchSig, pl.MakeSignString(), strError));
    pl.scriptPayout = GetRandomScript();
    BOOST_CHECK(!CMessageSigner::VerifyMessage(keyID, pl.vchSig, pl.MakeSignString(), strError));
}

BOOST_AUTO_TEST_CASE(llmqcomm_setpayload_test)
{
    const llmq::LLMQCommPL& pl = GetRandomLLMQCommPayload();

    CMutableTransaction mtx;
    SetTxPayload(mtx, pl);
    llmq::LLMQCommPL pl2;
    BOOST_CHECK(GetTxPayload(mtx, pl2));
    BOOST_CHECK(pl.nHeight == pl2.nHeight);
    BOOST_CHECK(EqualCommitments(pl.commitment, pl2.commitment));
}


BOOST_AUTO_TEST_SUITE_END()
