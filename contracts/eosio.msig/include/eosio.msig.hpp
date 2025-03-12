#pragma once

#include <eosio/action.hpp>
#include <eosio/crypto.hpp>
#include <eosio/permission.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/eosio.hpp>
#include <eosio/ignore.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;

class [[eosio::contract("eosio.msig")]] multisig : public eosio::contract {
  public:
    using eosio::contract::contract;

    [[eosio::action]] void propose(name proposer, name proposal_name, std::vector<permission_level> requested, ignore<transaction> trx);
    [[eosio::action]] void approve(name proposer, name proposal_name, permission_level level, const eosio::binary_extension<eosio::checksum256> & proposal_hash);
    [[eosio::action]] void unapprove(name proposer, name proposal_name, permission_level level);
    [[eosio::action]] void cancel(name proposer, name proposal_name, name canceler);
    [[eosio::action]] void exec(name proposer, name proposal_name, name executer);
    [[eosio::action]] void invalidate(name account);


    struct [[eosio::table("proposals")]] _proposals_s {
      name proposal_name;
      std::vector<char> packed_transaction;
      eosio::binary_extension<std::optional<time_point>> earliest_exec_time;

      uint64_t primary_key()const { return proposal_name.value; };
    };
    typedef multi_index<name("proposals"), _proposals_s> _proposals;

    struct approval {
      permission_level level;
      time_point time;
    };

    struct [[eosio::table("approvals")]] _approvals_s {
      uint8_t version = 1;
      name proposal_name;
      std::vector<approval> requested_approvals;
      std::vector<approval> provided_approvals;
      uint64_t primary_key()const { return proposal_name.value; }
    };
    typedef multi_index<name("approvals"), _approvals_s> _approvals;

    struct [[eosio::table("invalids")]] _invalids_s {
      name account;
      time_point last_invalidation_time;

      uint64_t primary_key() const { return account.value; };
    };
    typedef multi_index<name("invalids"), _invalids_s> _invalids;

  private:
    eosio::transaction_header get_trx_header(const char* ptr, size_t sz);
    bool trx_is_authorized(const std::vector<permission_level> & approvals, const std::vector<char> & packed_trx);

    template<typename Function>
    std::vector<permission_level> get_approvals_and_adjust_table(name self, name proposer, name proposal_name, Function&& table_op);
};

