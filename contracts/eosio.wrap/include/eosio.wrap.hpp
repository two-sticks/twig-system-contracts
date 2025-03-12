#pragma once

#include <eosio/eosio.hpp>
#include <eosio/ignore.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;

class [[eosio::contract("eosio.wrap")]] wrap : public eosio::contract {
  public:
    using eosio::contract::contract;

    [[eosio::action]] void exec(ignore<name> executer, ignore<transaction> trx);

    using exec_action = eosio::action_wrapper<"exec"_n, &wrap::exec>;
};