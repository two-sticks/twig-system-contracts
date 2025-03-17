#include <eosio.bank.hpp>

void bank::cleanup()
{
  require_auth(get_self());

  _vesting vesting(get_self(), get_self().value);

  auto vesting_itr = vesting.begin();
  while (vesting_itr != vesting.end()){
    vesting_itr = vesting.erase(vesting_itr);
  }
}

void bank::token_deposit(name from, name to, asset quantity, std::string memo)
{
  if (to != get_self() || from == get_self() || from != chunks_account || memo == "ignore_memo" || quantity.symbol != core_symbol){
    return;
  }

  systemcore::_aluckynumber aluckynumber_(system_account, system_account.value);
  auto aluckynumber = aluckynumber_.get();

  _vesting vesting(get_self(), get_self().value);

  name producer = memo == "team_share" ? team_account : memo == "producer_share" ? aluckynumber.producer : name(0);
  check(producer.value != 0, "invalid memo");

  auto vesting_itr = vesting.find(producer.value);
  if (vesting_itr == vesting.end()){
    vesting.emplace(get_self(), [&](auto & row){
      row.producer = producer;
      row.vested = quantity;
      row.unvesting_total = asset{0, quantity.symbol};
      row.unvested = asset{0, quantity.symbol};
      row.epoch = aluckynumber.epoch;
    });
  } else {
    vesting.modify(vesting_itr, eosio::same_payer, [&](auto & row){
      row.vested.amount += quantity.amount;
      if (vesting_itr->unvesting_total.amount > 0){
        double chunks_ratio = aluckynumber.chunks_remaining / systemcore::lucky_number_chunks;
        double inverse_paidout_ratio = ((vesting_itr->unvested.amount / vesting_itr->unvesting_total.amount) - 1) * -1;

        double current_payout_ratio = inverse_paidout_ratio - chunks_ratio;
        if (current_payout_ratio > 0){
          asset unvested_tokens = asset{int64_t(current_payout_ratio * (double)vesting_itr->unvesting_total.amount), quantity.symbol};

          // Fix rounding errors
          if (unvested_tokens.amount + vesting_itr->unvested.amount > vesting_itr->unvesting_total.amount){
            unvested_tokens.amount = vesting_itr->unvesting_total.amount - vesting_itr->unvested.amount;
          }

          eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
            std::make_tuple(get_self(), producer, unvested_tokens, (std::string)"Distributing unvested production rewards for "+ producer.to_string())).send();

          row.unvested.amount += unvested_tokens.amount;
        }
      }
    });
  }
}

void bank::onepoch(uint32_t epoch)
{
  require_auth(get_self());

  systemcore::_aluckynumber aluckynumber_(system_account, system_account.value);
  auto aluckynumber = aluckynumber_.get();

  check(aluckynumber.epoch == epoch, "invalid epoch");

  _vesting vesting(get_self(), get_self().value);
  auto vesting_itr = vesting.begin();
  while (vesting_itr != vesting.end()){
    if (vesting_itr->epoch + 1 == epoch){
      asset finished_vesting = asset{vesting_itr->unvesting_total.amount - vesting_itr->unvested.amount, vesting_itr->vested.symbol};
      if (finished_vesting.amount > 0){
        eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
        std::make_tuple(get_self(), vesting_itr->producer, finished_vesting, (std::string)"Distributing end of Epoch rewards...")).send();
      }
      vesting.modify(vesting_itr, eosio::same_payer, [&](auto & row){
        row.unvesting_total.amount = vesting_itr->vested.amount;
        row.unvested.amount = 0;
        row.vested.amount = 0;

        row.epoch = epoch;
      });
    }

    ++vesting_itr;
  }
}