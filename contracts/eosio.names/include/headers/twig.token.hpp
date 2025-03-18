#pragma once

#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

using namespace eosio;
namespace token
{
  typedef std::map<eosio::symbol, int64_t> fungible_pairs;

  struct _supplies_s
  {
    fungible_pairs supply;
    fungible_pairs max_supply;
  };
  typedef singleton<name("supplies"), _supplies_s> _supplies;

  struct _accounts_s
  {
    name owner;
    fungible_pairs balance;

    uint64_t primary_key() const { return (uint64_t)owner.value; };
  };
  typedef multi_index<name("accounts"), _accounts_s> _accounts;
};
