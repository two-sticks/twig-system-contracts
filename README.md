# TWIG System Contracts (WORK IN PROGRESS)

System contracts tailored & refactored for use on a TWIG fork of Antelope Spring.

Implements a lot of critical functionality that goes beyond what is provided by the base Antelope protocol.

This collection consists of the following individual contracts:

* [boot contract](contracts/eosio.boot/include/eosio.boot.hpp): A minimal contract that only serves the purpose of activating protocol features which enables other more sophisticated contracts to be deployed onto the blockchain. (Note: this contract must be deployed to the privileged `eosio` account.)
* [bios contract](contracts/eosio.bios/include/eosio.bios.hpp): A simple alternative to the core contract which is suitable for test chains or perhaps centralized blockchains. (Note: this contract must be deployed to the privileged `eosio` account.)
* [token contract](contracts/eosio.token/include/eosio.token.hpp): A contract enabling fungible tokens.
* [system contract](contracts/eosio.system/include/eosio.system.hpp): A monolithic contract that includes a variety of different functions which enhances a base Antelope blockchain for use as a public, decentralized blockchain in an opinionated way. (Note: This contract must be deployed to the privileged `eosio` account. Additionally, this contract requires that the token contract is deployed to the `eosio.token` account and has already been used to setup the core token.) The functions contained within this monolithic contract include (non-exhaustive):
   + Delegated Proof of Stake (DPoS) consensus mechanism for selecting and paying (via core token inflation) a set of block producers that are chosen through delegation of the staked core tokens.
   + Allocation of CPU/NET resources based on core tokens in which the core tokens are either staked for an indefinite allocation of some fraction of available CPU/NET resources, or they are paid as a fee in exchange for a time-limited allocation of CPU/NET resources via REX or via PowerUp.
   + An automated market maker enabling a market for RAM resources which allows users to buy or sell available RAM allocations.
   + An auction for bidding for premium account names.
* [multisig contract](contracts/eosio.msig/include/eosio.msig.hpp): A contract that enables proposing Antelope transactions on the blockchain, collecting authorization approvals for many accounts, and then executing the actions within the transaction after authorization requirements of the transaction have been reached. (Note: this contract must be deployed to a privileged account.)
* [wrap contract](contracts/eosio.wrap/include/eosio.wrap.hpp): A contract that wraps around any Antelope transaction and allows for executing its actions without needing to satisfy the authorization requirements of the transaction. If used, the permissions of the account hosting this contract should be configured to only allow highly trusted parties (e.g. the operators of the blockchain) to have the ability to execute its actions. (Note: this contract must be deployed to a privileged account.)

## Compilation

Precompiled & built with AntelopeCDT V4.1.0 using

```bash
cdt-cpp -O3 -lto-opt=O3 -fmerge-all-constants -faligned-allocation --no-missing-ricardian-clause -abigen -I include -contract %CONTRACT_NAME% -o %CONTRACT_NAME%.wasm src/%MAIN_FILE%
```