# TWIG System Contracts (WORK IN PROGRESS)

System contracts tailored & refactored for use on a TWIG fork of Antelope Spring.

Implements a lot of critical functionality that goes beyond what is provided by the base Antelope protocol.

This collection consists of the following individual contracts:

* [boot contract](contracts/eosio.boot/include/eosio.boot.hpp): For starting up the systems contract.
* [token contract](contracts/eosio.token/include/eosio.token.hpp): A contract enabling fungible tokens.
* [system contract](contracts/eosio.system/include/eosio.system.hpp): A monolithic contract that includes a variety of different functions which enhances a base Antelope blockchain for use as a public, decentralized blockchain in an opinionated way. The functions contained within this monolithic contract include (non-exhaustive):
   + Delegated Proof of Stake (DPoS) consensus mechanism for selecting and paying (via core token inflation) a set of block producers that are chosen through delegation of the staked core tokens.
   + An auction for bidding for premium account names.
* [multisig contract](contracts/eosio.msig/include/eosio.msig.hpp): A contract that enables proposing Antelope transactions on the blockchain, collecting authorization approvals for many accounts, and then executing the actions within the transaction after authorization requirements of the transaction have been reached.
* [wrap contract](contracts/eosio.wrap/include/eosio.wrap.hpp): A contract that wraps around any Antelope transaction and allows for executing its actions without needing to satisfy the authorization requirements of the transaction. If used, the permissions of the account hosting this contract should be configured to only allow highly trusted parties (e.g. the operators of the blockchain) to have the ability to execute its actions.

## Compilation

Precompiled & built with AntelopeCDT V4.1.0 using

```bash
cdt-cpp -O3 -lto-opt=O3 -fmerge-all-constants -faligned-allocation --no-missing-ricardian-clause -abigen -I include -contract %CONTRACT_NAME% -o %CONTRACT_NAME%.wasm src/%MAIN_FILE%
```