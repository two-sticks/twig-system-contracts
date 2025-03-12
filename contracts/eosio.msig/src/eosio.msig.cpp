#include <eosio.msig.hpp>

eosio::transaction_header multisig::get_trx_header(const char* ptr, size_t sz)
{
  datastream<const char*> ds = {ptr, sz};
  transaction_header trx_header;
  ds >> trx_header;
  return trx_header;
}

bool multisig::trx_is_authorized(const std::vector<permission_level> & approvals, const std::vector<char> & packed_trx)
{
  auto packed_approvals = pack(approvals);
  return check_transaction_authorization(packed_trx.data(), packed_trx.size(), (const char*)0, 0, packed_approvals.data(), packed_approvals.size());
}

template<typename Function>
std::vector<permission_level> multisig::get_approvals_and_adjust_table(name self, name proposer, name proposal_name, Function&& table_op)
{
  std::vector<permission_level> approvals_vector;
  _invalids invalids(self, self.value);

  _approvals approvals(self, proposer.value);
  auto approvals_itr = approvals.find(proposal_name.value);

  if(approvals_itr != approvals.end()){
    approvals_vector.reserve(approvals_itr->provided_approvals.size());
    for(const auto & permission : approvals_itr->provided_approvals){
      auto invalid_itr = invalids.find(permission.level.actor.value);
      if (invalid_itr == invalids.end() || invalid_itr->last_invalidation_time < permission.time){
        approvals_vector.push_back(permission.level);
      }
    }
    table_op(approvals, approvals_itr);
   }
  return approvals_vector;
}

void multisig::propose(name proposer, name proposal_name, std::vector<permission_level> requested, ignore<transaction> trx)
{
  require_auth( proposer );
  auto& ds = get_datastream();

  const char* trx_pos = ds.pos();
  size_t size = ds.remaining();

  transaction_header trx_header;
  std::vector<action> context_free_actions;
  ds >> trx_header;
  check(trx_header.expiration >= eosio::time_point_sec(current_time_point()), "transaction expired");
  ds >> context_free_actions;
  check(context_free_actions.empty(), "not allowed to `propose` a transaction with context-free actions");

  _proposals proposals(get_self(), proposer.value);
  check(proposals.find(proposal_name.value) == proposals.end(), "proposal with the same name exists");

  auto packed_requested = pack(requested);
  auto res = check_transaction_authorization(trx_pos, size, (const char*)0, 0, packed_requested.data(), packed_requested.size());
  check(res > 0, "transaction authorization failed");

  std::vector<char> pkd_trans;
  pkd_trans.resize(size);
  memcpy((char*)pkd_trans.data(), trx_pos, size);

  proposals.emplace(proposer, [&](auto & row){
    row.proposal_name = proposal_name;
    row.packed_transaction = pkd_trans;
    row.earliest_exec_time.emplace();
  });

  _approvals approvals(get_self(), proposer.value);
  approvals.emplace(proposer, [&](auto & row){
    row.proposal_name = proposal_name;
    row.requested_approvals.reserve( requested.size() );
    for (auto & level : requested){
      row.requested_approvals.push_back( approval{ level, time_point{ microseconds{0} } } );
    }
  });
}

void multisig::approve(name proposer, name proposal_name, permission_level level, const eosio::binary_extension<eosio::checksum256> & proposal_hash)
{
  require_auth(level);

  _proposals proposals(get_self(), proposer.value);
  auto & proposals_itr = proposals.get(proposal_name.value, "proposal not found");

  if(proposal_hash){
    assert_sha256(proposals_itr.packed_transaction.data(), proposals_itr.packed_transaction.size(), *proposal_hash );
  }

  _approvals approvals(get_self(), proposer.value);
  auto approvals_itr = approvals.find(proposal_name.value);
  if (approvals_itr != approvals.end()){
    auto itr = std::find_if(approvals_itr->requested_approvals.begin(), approvals_itr->requested_approvals.end(), [&](const approval & a) { return a.level == level; } );
    check(itr != approvals_itr->requested_approvals.end(), "approval is not on the list of requested approvals");

    approvals.modify(approvals_itr, proposer, [&](auto & row){
      row.provided_approvals.push_back(approval{ level, current_time_point() });
      row.requested_approvals.erase(itr);
    });
  } else {
    check(false, "approval is not on the list of requested approvals");
  }

  transaction_header trx_header = get_trx_header(proposals_itr.packed_transaction.data(), proposals_itr.packed_transaction.size());

  if(proposals_itr.earliest_exec_time.has_value()){
    if(!proposals_itr.earliest_exec_time->has_value()){
      auto table_op = [](auto&&, auto&&){};
      if(trx_is_authorized(get_approvals_and_adjust_table(get_self(), proposer, proposal_name, table_op), proposals_itr.packed_transaction)){
        proposals.modify(proposals_itr, proposer, [&](auto & row){
          row.earliest_exec_time.emplace(time_point{ current_time_point() + eosio::seconds(trx_header.delay_sec.value)});
        });
      }
    }
  } else {
    check(trx_header.delay_sec.value == 0, "old proposals are not allowed to have non-zero `delay_sec`; cancel and retry" );
  }
}

void multisig::unapprove(name proposer, name proposal_name, permission_level level)
{
  require_auth(level);

  _approvals approvals(get_self(), proposer.value);
  auto approvals_itr = approvals.find(proposal_name.value);
  if (approvals_itr != approvals.end()){
    auto itr = std::find_if(approvals_itr->provided_approvals.begin(), approvals_itr->provided_approvals.end(), [&](const approval & a) { return a.level == level; } );
    check(itr != approvals_itr->provided_approvals.end(), "no approval previously granted");
    approvals.modify(approvals_itr, proposer, [&](auto & row){
      row.requested_approvals.push_back(approval{ level, current_time_point()});
      row.provided_approvals.erase(itr);
    });
  } else {
    check(false, "no approval previously granted");
  }

  _proposals proposals(get_self(), proposer.value);
  auto & proposals_itr = proposals.get(proposal_name.value, "proposal not found");

  if(proposals_itr.earliest_exec_time.has_value()){
    if(proposals_itr.earliest_exec_time->has_value()){
      auto table_op = [](auto&&, auto&&){};
      if(!trx_is_authorized(get_approvals_and_adjust_table(get_self(), proposer, proposal_name, table_op), proposals_itr.packed_transaction)){
        proposals.modify(proposals_itr, proposer, [&](auto & row){
          row.earliest_exec_time.emplace();
        });
      }
    }
  } else {
    transaction_header trx_header = get_trx_header(proposals_itr.packed_transaction.data(), proposals_itr.packed_transaction.size());
    check(trx_header.delay_sec.value == 0, "old proposals are not allowed to have non-zero `delay_sec`; cancel and retry");
  }
}

void multisig::cancel(name proposer, name proposal_name, name canceler)
{
  require_auth(canceler);

  _proposals proposals(get_self(), proposer.value);
  auto & proposals_itr = proposals.get(proposal_name.value, "proposal not found");

  if(canceler != proposer) {
    check(unpack<transaction_header>(proposals_itr.packed_transaction ).expiration < eosio::time_point_sec(current_time_point()), "cannot cancel until expiration" );
  }
  proposals.erase(proposals_itr);

  _approvals approvals(get_self(), proposer.value);
  auto approvals_itr = approvals.find(proposal_name.value);
  if (approvals_itr != approvals.end() ) {
    approvals.erase(approvals_itr);
  } else {
    check(false, "approval not found");
  }
}

void multisig::exec(name proposer, name proposal_name, name executer)
{
  require_auth(executer);

  _proposals proposals(get_self(), proposer.value);
  auto & proposals_itr = proposals.get(proposal_name.value, "proposal not found");
  transaction_header trx_header;
  std::vector<action> context_free_actions;
  std::vector<action> actions;
  datastream<const char*> ds(proposals_itr.packed_transaction.data(), proposals_itr.packed_transaction.size());
  ds >> trx_header;
  check(trx_header.expiration >= eosio::time_point_sec(current_time_point()), "transaction expired");
  ds >> context_free_actions;
  check(context_free_actions.empty(), "not allowed to `exec` a transaction with context-free actions");
  ds >> actions;

  auto table_op = [](auto&& table, auto&& table_iter) { table.erase(table_iter); };
  bool ok = trx_is_authorized(get_approvals_and_adjust_table(get_self(), proposer, proposal_name, table_op), proposals_itr.packed_transaction);
  check(ok, "transaction authorization failed");

  if (proposals_itr.earliest_exec_time.has_value() && proposals_itr.earliest_exec_time->has_value()){
    check(**proposals_itr.earliest_exec_time <= current_time_point(), "too early to execute");
  } else {
    check(trx_header.delay_sec.value == 0, "old proposals are not allowed to have non-zero `delay_sec`; cancel and retry");
  }

  for (const auto & act : actions){
    act.send();
  }

  proposals.erase(proposals_itr);
}

void multisig::invalidate(name account)
{
  require_auth(account);
  _invalids invalids(get_self(), get_self().value);
  auto invalids_itr = invalids.find(account.value);
  if(invalids_itr == invalids.end()){
    invalids.emplace(account, [&](auto & row){
      row.account = account;
      row.last_invalidation_time = current_time_point();
    });
  } else {
    invalids.modify(invalids_itr, account, [&](auto & row){
      row.last_invalidation_time = current_time_point();
    });
  }
}

