#pragma once

#include <eosio/action.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/permission.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/datastream.hpp>
#include <eosio/dispatcher.hpp>
#include <eosio/ignore.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/serialize.hpp>

#include <headers/eosio.token.hpp>
#include <headers/eosio.system.hpp>

using namespace eosio;

class [[eosio::contract("eosio.bank")]] bank : public eosio::contract {
  public:
    using eosio::contract::contract;

    static constexpr eosio::name system_account{"eosio"_n};
    static constexpr eosio::name token_account{"eosio.token"_n};
    static constexpr eosio::name chunks_account{"eosio.chunks"_n};

    static constexpr eosio::name team_account{"eosio.twig"_n};

    [[eosio::on_notify("eosio.token::transfer")]] void token_deposit(name from, name to, asset quantity, std::string memo);
    [[eosio::action]] void onepoch(uint32_t epoch);

// TABLES
    struct [[eosio::table("vesting")]] _vesting_s
    {
      name producer;
      asset vested; // starts 0, builds up, empties into unvesting_total at end of epoch
      asset unvesting_total; // upon epoch, vested -> unvesting_total,
      asset unvested; // on new production, sends out from unvested

      uint32_t epoch;

      uint64_t primary_key() const { return producer.value; };
    };
    typedef multi_index<name("vesting"), _vesting_s>_vesting;
};