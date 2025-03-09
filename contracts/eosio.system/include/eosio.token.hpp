#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;
namespace token
{
  // Accounts Table
  struct _accounts_s {
    asset balance;

    uint64_t primary_key() const { return balance.symbol.code().raw(); };
  };
  typedef eosio::multi_index<name("accounts"), _accounts_s > _accounts;

  // Stats Table
  struct _stats_s {
      asset supply;
      asset max_supply;
      name issuer;

      uint64_t primary_key() const { return supply.symbol.code().raw(); };
  };
  typedef eosio::multi_index<name("stat"), _stats_s > _stats;

  // Functions
  static asset get_supply(name token_contract_account, symbol_code sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").supply;
  }

  static asset get_max_supply(name token_contract_account, symbol_code sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").max_supply;
  }

  static name get_issuer(name token_contract_account, symbol_code sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").issuer;
  }

  static asset get_balance(name token_contract_account, name owner, symbol_code sym_code){
      _accounts accountstable(token_contract_account, owner.value);
      return accountstable.get(sym_code.raw(), "no balance with specified symbol").balance;
  }
};
