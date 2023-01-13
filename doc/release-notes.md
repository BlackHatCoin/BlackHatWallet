(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

BlackHat Core version *version* is now available from:  <https://github.com/BlackHatCoin/BlackHatWallet/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/BlackHatCoin/BlackHatWallet/issues>


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/BLKC-Qt (on Mac) or blkcd/blkc-qt (on Linux).

Notable Changes
==============

(Developers: add your notes here as part of your pull requests whenever possible)

Network Activity Toggle
-----------------------

The GUI wallet now allows for the user to enable/disable all network activity from the Settings page. This can be handy if, for example, you need to restart your router or modem.

RPC Changes
-----------

### getblock additional verbosity

The `getblock` command now has an additional verbosity level. The command's second parameter has been changed from a boolean to and integer to support this.

- If verbosity is 0, returns a string that is serialized, hex-encoded data for block 'hash'.
- If verbosity is 1, returns an Object with information about block `hash`.
- If verbosity is 2, returns an Object with information about block `hash` and information about each transaction.

### getnewshieldaddress label support

The `getnewshieldaddress` command now supports an optional `label` parameter. If specified, the text label will be added to the wallet's address book.

### New setnetworkactive command

A new `setnetworkactive` command has been added to enable/disable all network activity. The command specs are as follows:

```
setnetworkactive true|false
Enable/Disable all p2p network activity.

Result:
status: (boolean) The final network activity status

Examples:
setnetworkactive true
setnetworkactive false
```

### New scantxoutset command

A new `scantxoutset` command has been added that allows for advanced searching of the unspent transaction outputs (UTXOs). As this is a highly advanced and complex command that won't apply to most end users, the full command spec won't be detailed here.

Please refer to the detailed spec by running `help scantxoutset`.

*version* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features

### Build System

### P2P Protocol and Network Code

### GUI

### RPC/REST

### Wallet

### Miscellaneous

## Credits

