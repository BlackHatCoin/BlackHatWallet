#!/usr/bin/env python3
# Copyright (c) 2024 The PIVX Core developers
# Copyright (c) 2021-2024 The BlackHat developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test OP_EXCHANGEADDR.

Test that the OP_EXCHANGEADDR soft-fork activates at (regtest) block height
1001, and that the following is capable
    t > e
    e > t
    e > s
and not capable
    s > e
"""

from decimal import Decimal

from test_framework.test_framework import BlackHatTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.script import CScript, OP_NOP, OP_CHECKSIG
from test_framework.messages import CTransaction, CTxIn, CTxOut, COutPoint, ToHex

FEATURE_PRE_SPLIT_KEYPOOL = 169900

class ExchangeAddrTest(BlackHatTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [['-whitelist=127.0.0.1','-regtest'], ['-regtest']]
        self.setup_clean_chain = True

    def run_test(self):
        # Mine and test Pre v5.6 bad OP_EXCHANGEADDR
        self.nodes[0].generate(800)
        address = self.nodes[0].getnewaddress()
        self.nodes[0].sendtoaddress(address, 10)
        self.nodes[0].generate(6)
        utxo = self.nodes[0].listunspent()[0]
        # Create a raw transaction that attempts to spend the UTXO with a custom opcode
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), b""))
        tx.vout.append(CTxOut(int(utxo["amount"] * 100000000) - 10000, CScript([OP_NOP, OP_CHECKSIG])))
        tx.vin[0].scriptSig = CScript([b'\xe0', b'\x51'])  # OP_EXCHANGEADDR (0xe0) followed by OP_1 (0x51)

        # Sign the transaction
        signed_tx = self.nodes[0].signrawtransaction(ToHex(tx))

        # Before the upgrade, this should fail if OP_EXCHANGEADDR is disallowed
        error_code = -26
        error_message = "scriptpubkey"
        assert_raises_rpc_error(error_code, error_message, self.nodes[0].sendrawtransaction, signed_tx["hex"])

        # Test that proper Exchange Address is not allowed before upgrade
        ex_addr = self.nodes[1].getnewexchangeaddress()

        # Attempt to send funds from transparent address to exchange address
        ex_addr_validation_result = self.nodes[0].validateaddress(ex_addr)
        assert_equal(ex_addr_validation_result['isvalid'], True)
        # This should succeed even before the upgrade
        self.nodes[0].sendtoaddress(ex_addr, 1.0)

        # Check wallet version
        wallet_info = self.nodes[0].getwalletinfo()
        if wallet_info['walletversion'] >= FEATURE_PRE_SPLIT_KEYPOOL:
            sapling_addr = self.nodes[0].getnewshieldaddress()
            self.nodes[0].sendtoaddress(sapling_addr, 2.0)
            self.nodes[0].generate(1)
            sap_to_ex = [{"address": ex_addr, "amount": Decimal('1')}]
            # Shield data should be allowed before activation
            self.nodes[0].shieldsendmany(sapling_addr, sap_to_ex)
        else:
            self.nodes[0].generate(1)
        # Mine and activate exchange addresses
        self.nodes[0].generate(193)
        assert_equal(self.nodes[0].getblockcount(), 1000)
        self.nodes[0].generate(1)

        # Get addresses for testing
        ex_addr_2 = self.nodes[1].getnewexchangeaddress()
        t_addr_2 = self.nodes[0].getnewaddress()

        # Attempt to send funds from transparent address to exchange address pre-upgrade activation
        self.nodes[0].sendtoaddress(ex_addr_2, 2.0)
        self.nodes[0].generate(6)
        self.sync_all()


        # Verify balance
        node_bal = self.nodes[1].getbalance()
        if wallet_info['walletversion'] >= FEATURE_PRE_SPLIT_KEYPOOL:
            assert_equal(node_bal, 4)
        else:
            assert_equal(node_bal, 3)
        # Attempt to send funds from exchange address back to transparent address
        tx2 = self.nodes[0].sendtoaddress(t_addr_2, 1.0)
        self.nodes[0].generate(6)
        self.sync_all()
        ex_result = self.nodes[0].gettransaction(tx2)

        # Assert that the transaction ID in the result matches tx2
        assert_equal(ex_result['txid'], tx2)

        # Transparent to Shield to Exchange should fail
        # Check wallet version
        if wallet_info['walletversion'] < FEATURE_PRE_SPLIT_KEYPOOL:
            self.log.info("Pre-HD wallet version detected. Skipping Shield tests.")
            return
        sapling_addr = self.nodes[0].getnewshieldaddress()
        self.nodes[0].sendtoaddress(sapling_addr, 2.0)
        self.nodes[0].generate(6)
        sap_to_ex = [{"address": ex_addr, "amount": Decimal('1')}]

        # Expect shieldsendmany to fail with bad-txns-invalid-sapling
        expected_error_code = -4
        expected_error_message = "Failed to accept tx in the memory pool (reason: bad-txns-exchange-addr-has-sapling)"
        assert_raises_rpc_error(
            expected_error_code,
            expected_error_message,
            self.nodes[0].shieldsendmany,
            sapling_addr, sap_to_ex, 1, 0, sapling_addr
        )

        # Generate a new shielded address
        new_shielded_addr = self.nodes[1].getnewshieldaddress()

        # Send to Shield from Exchange Addr
        tx3 = self.nodes[1].sendtoaddress(new_shielded_addr, 1.0)
        ex_result2 = self.nodes[1].gettransaction(tx3)
        assert_equal(ex_result2['txid'], tx3)


if __name__ == "__main__":
    ExchangeAddrTest().main()

