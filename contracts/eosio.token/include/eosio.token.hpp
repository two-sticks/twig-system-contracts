#pragma once

#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>

#include <string>

using namespace eosio;
using std::string;

class [[eosio::contract("eosio.token")]] token : public eosio::contract {
  public:
    using eosio::contract::contract;

    // System Actions
    [[eosio::action]] void create(const name & issuer, const asset & maximum_supply);
    [[eosio::action]] void issue(const name & to, const asset & quantity, const string & memo);
    [[eosio::action]] void issuefixed(const name & to, const asset & supply, const string & memo);
    [[eosio::action]] void setmaxsupply(const name & issuer, const asset & maximum_supply);
    [[eosio::action]] void retire(const asset & quantity, const string & memo);

    // User Actions
    [[eosio::action]] void transfer(const name & from, const name & to, const asset & quantity, const string & memo);
    [[eosio::action]] void open(const name & owner, const symbol & symbol, const name & ram_payer);
    [[eosio::action]] void close(const name & owner, const symbol & symbol);

    // Accounts Table
    struct [[eosio::table("accounts")]] _accounts_s {
      asset balance;

      uint64_t primary_key() const { return balance.symbol.code().raw(); };
    };
    typedef multi_index<name("accounts"), _accounts_s> _accounts;

    // Stats Table
    struct [[eosio::table("stat")]] _stats_s {
      asset supply;
      asset max_supply;
      name issuer;

      uint64_t primary_key() const { return supply.symbol.code().raw(); };
    };
    typedef multi_index<name("stat"), _stats_s> _stats;

    // Functions
    static asset get_supply(const name & token_contract_account, const symbol_code & sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").supply;
    }

    static asset get_max_supply(const name & token_contract_account, const symbol_code & sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").max_supply;
    }

    static name get_issuer(const name & token_contract_account, const symbol_code & sym_code){
      _stats statstable(token_contract_account, sym_code.raw());
      return statstable.get(sym_code.raw(), "invalid supply symbol code").issuer;
    }

    static asset get_balance(const name & token_contract_account, const name & owner, const symbol_code & sym_code){
      _accounts accountstable(token_contract_account, owner.value);
      return accountstable.get(sym_code.raw(), "no balance with specified symbol").balance;
    }

  private:
    void sub_balance(const name & owner, const asset & value);
    void add_balance(const name & owner, const asset & value, const name & ram_payer);
};
