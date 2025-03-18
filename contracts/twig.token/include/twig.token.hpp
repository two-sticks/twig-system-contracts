#pragma once

#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

#include <string>
#include <headers/RandomnessProvider.hpp>

using namespace eosio;
using std::string;

class [[eosio::contract("twig.token")]] token : public eosio::contract {
  public:
    using eosio::contract::contract;

    static constexpr symbol core_symbol = symbol(symbol_code("TWIG"), 4);

    typedef std::map<eosio::symbol, int64_t> fungible_pairs;
    typedef std::vector<int32_t> i32v;
    typedef std::vector<uint64_t> u64v;

    struct branding_meta
    {
      std::string x64;
      std::string x256;
      std::string x1024;
      std::string svg;
      std::map<std::string, std::string> extras;
      EOSLIB_SERIALIZE(branding_meta, (x64)(x256)(x1024)(svg)(extras))
    };

    struct fungible_meta : branding_meta
    {
      std::string name;
      std::string desc;
      i32v tags;
      EOSLIB_SERIALIZE_DERIVED(fungible_meta, branding_meta, (name)(desc)(tags))
    };
    typedef std::map<eosio::symbol, fungible_meta> fungible_params;

    struct fungible_rng
    {
      uint32_t odds;
      eosio::symbol sym;
      int64_t quant;
      i32v tags;
      EOSLIB_SERIALIZE(fungible_rng, (odds)(sym)(quant)(tags))
    };
    typedef std::vector<fungible_rng> fungible_drops;

    struct drop_params
    {
      u64v droptable;
      i32v quant;
      i32v juice;
      i32v cds;
      std::map<int32_t, double> tag_weights;
      EOSLIB_SERIALIZE(drop_params, (droptable)(quant)(juice)(cds)(tag_weights))
    };

    struct config_params
    {
      symbol core_symbol = symbol(symbol_code("TWIG"), 4);
      std::vector<symbol> preserved;
    };

    // Contract Config Actions
    [[eosio::action]] void cfginit(const std::string & memo);
    [[eosio::action]] void cfgdestruct(const std::string & memo);
    [[eosio::action]] void cfgsetparams(const config_params & params, const std::string & memo);

    [[eosio::action]] void setfungibles(const fungible_params & params, const std::string & memo);
    [[eosio::action]] void setsupplies(const fungible_pairs & max_supply, const std::string & memo);
    [[eosio::action]] void rmvsupply(const fungible_pairs & max_supply, const std::string & memo); // Testing Action
    [[eosio::action]] void setdrops(const uint64_t index, const fungible_drops & params, const std::string & memo);
    [[eosio::action]] void rmvdrops(const uint64_t index);

    // Contract Admin Actions
    [[eosio::action]] void issue(const name & owner, const fungible_pairs & tokens, const std::string & memo);
    [[eosio::action]] void retire(const name & owner, const fungible_pairs & tokens, const std::string & memo); // variant needed for paying with fees, routing ->

    [[eosio::action]] void rolldrops(const name & owner, const drop_params & params);
    [[eosio::action]] void execdrops(const checksum256 & seed);

    // User Actions
    [[eosio::action]] void open(const name & owner);
    [[eosio::action]] void close(const name & owner);
    [[eosio::action]] void claimdrops(const u64v & indices, const name & owner);
    [[eosio::action]] void transfer(const name & from, const name & to, const std::vector<asset> & tokens, const string & memo);

// Contract Tables
    struct [[eosio::table("config")]] _config_s
    {
      config_params params;
    };
    typedef singleton<name("config"), _config_s> _config;

    struct [[eosio::table("fungibles")]] _fungibles_s
    {
      fungible_params params;
    };
    typedef singleton<name("fungibles"), _fungibles_s> _fungibles;

    struct [[eosio::table("supplies")]] _supplies_s
    {
      fungible_pairs supply;
      fungible_pairs max_supply;
    };
    typedef singleton<name("supplies"), _supplies_s> _supplies;

    struct [[eosio::table("drops")]] _drops_s
    {
      uint64_t index;
      fungible_drops params;

      uint64_t primary_key() const { return index; };
    };
    typedef multi_index<name("drops"), _drops_s> _drops;

    struct [[eosio::table("rolldrops")]] _rolldrops_s
    {
      uint64_t index;
      name owner;
      drop_params params;

      uint64_t primary_key() const { return (uint64_t)index; };
    };
    typedef multi_index<name("rolldrops"), _rolldrops_s> _rolldrops;

// User Tables
    struct [[eosio::table("accounts")]] _accounts_s
    {
      name owner;
      fungible_pairs balance;

      uint64_t primary_key() const { return (uint64_t)owner.value; };
    };
    typedef multi_index<name("accounts"), _accounts_s> _accounts;

    struct [[eosio::table("unboxed")]] _unboxed_s
    {
      uint64_t index;
      name owner;
      fungible_pairs tokens;

      uint64_t primary_key() const { return index; };
      uint64_t secondary_key() const { return (uint64_t)owner.value; };

    };
    typedef multi_index<name("unboxed"), _unboxed_s,
      indexed_by<name("owners"), const_mem_fun<_unboxed_s, uint64_t, &_unboxed_s::secondary_key>>
    > _unboxed;
};
