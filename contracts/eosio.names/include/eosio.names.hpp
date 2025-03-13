#pragma once

#include <eosio/action.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/permission.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/ignore.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

#include <headers/eosio.token.hpp>

using namespace eosio;

class [[eosio::contract("eosio.names")]] namebidding : public eosio::contract {
  public:
    using eosio::contract::contract;

    static constexpr eosio::name token_account{"eosio.token"_n};
    static constexpr eosio::name chunks_account{"eosio.chunks"_n};

// CONFIG & ADMIN ACTIONS
    [[eosio::action]] void init(bool destruct, const symbol & core_symbol, const std::string & memo);
    [[eosio::action]] void exec();
    [[eosio::action]] void cleanup(const name & newname);

// USER ACTIONS
    [[eosio::action]] void bidname(const name & bidder, const name & newname, const asset & bid);
    [[eosio::action]] void bidrefund(const name & bidder, const name & newname);

// TABLES
    struct [[eosio::table("config")]] _config_s
    {
      symbol core_symbol;

    };
    typedef singleton<name("config"), _config_s> _config;

    struct [[eosio::table("namebids")]] _namebids_s
    {
      name newname;
      name high_bidder;
      asset high_bid; ///< negative high_bid == closed auction waiting to be claimed
      time_point last_bid_time;

      uint64_t primary_key() const { return newname.value; };
      uint64_t by_high_bid() const { return static_cast<uint64_t>(-high_bid.amount); };
    };
    typedef multi_index<name("namebids"), _namebids_s,
      indexed_by<name("highbid"), const_mem_fun<_namebids_s, uint64_t, &_namebids_s::by_high_bid>>
    >_namebids;

    struct [[eosio::table("bidrefunds")]] _bidrefunds_s
    {
      name bidder;
      asset amount;

      uint64_t primary_key()const { return bidder.value; };
    };
    typedef multi_index<name("bidrefunds"), _bidrefunds_s> _bidrefunds;
};