#include <eosio.names.hpp>

void namebidding::init(bool destruct, const symbol & core_symbol, const std::string & memo)
{
  require_auth(get_self());
  _config config(get_self(), get_self().value);

  if (destruct){
    config.remove();
  } else {
    token::get_max_supply(token_account, core_symbol.code()); // Used as checker for existance of token contract
    config.set(_config_s{
      .core_symbol = core_symbol
    }, get_self());
  }
}

void namebidding::exec(){
  require_auth(get_self());

  _namebids namebids(get_self(), get_self().value);
  auto namebids_by_bid = namebids.get_index<name("highbid")>();
  auto highest_bid = namebids_by_bid.lower_bound(std::numeric_limits<uint64_t>::max()/2);

  /* To do -> wtf is happening here?
  if(highest_bid != namebids_by_bid.end() &&
      highest_bid->high_bid > 0 &&
      (current_time_point() - highest_bid->last_bid_time) > microseconds(useconds_per_day) &&
      _gstate.thresh_activated_stake_time > time_point() &&
      (current_time_point() - _gstate.thresh_activated_stake_time) > microseconds(14 * useconds_per_day)
  ) {
      _gstate.last_name_close = timestamp;

      // logging
      native::logsystemfee_action logsystemfee_act{ get_self(), { {get_self(), active_permission} } };
      logsystemfee_act.send( get_self(), asset( highest_bid->high_bid, core_symbol() ), "buy name" );

      idx.modify( highest, same_payer, [&]( auto& b ){
        b.high_bid = -b.high_bid;
      });
  }
  */
  check(false, "currently disabled");
  /*
  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
  std::make_tuple(
    get_self(),
    chunks_account,
    bidrefunds_itr->amount,
    std::string("purchased name ")+ newname.to_string()
  )).send();
  */
}

void namebidding::cleanup(const name & newname)
{
  require_auth(get_self());

  _namebids namebids(get_self(), get_self().value);
  auto namebids_itr = namebids.require_find(newname.value, "newname not found");

  namebids.erase(namebids_itr);
}

void namebidding::bidname(const name & bidder, const name & newname, const asset & bid)
{
  require_auth(bidder);
  check(newname.suffix() == newname, "you can only bid on top-level suffix");

  _config config(get_self(), get_self().value);
  auto got_config = config.get();

  check((bool)newname, "the empty name is not a valid account name to bid on");
  check((newname.value & 0xFull) == 0, "13 character names are not valid account names to bid on");
  check((newname.value & 0x1F0ull) == 0, "accounts with 12 character names and no dots can be created without bidding required");
  check(!is_account(newname), "account already exists");
  check(bid.symbol == got_config.core_symbol, "asset must be system token");
  check(bid.amount > 0, "insufficient bid");

  // Pushes directly
  eosio::action(permission_level{bidder, name("active")}, token_account, name("transfer"),
    std::make_tuple(
      bidder,
      get_self(),
      bid,
      std::string("bid name ")+ newname.to_string()
    )).send();

  _namebids namebids(get_self(), get_self().value);
  auto namebids_itr = namebids.find(newname.value);

  if(namebids_itr == namebids.end()){
    namebids.emplace(bidder, [&](auto & row){
      row.newname = newname;
      row.high_bidder = bidder;
      row.high_bid = bid;
      row.last_bid_time = current_time_point();
    });
  } else {
    check(namebids_itr->high_bid.amount > 0, "this auction has already closed");
    check(bid.amount - namebids_itr->high_bid.amount > (namebids_itr->high_bid.amount / 10), "must increase bid by 10%");
    check(namebids_itr->high_bidder != bidder, "account is already highest bidder");

    _bidrefunds bidrefunds(get_self(), newname.value);
    auto bidrefunds_itr = bidrefunds.find(namebids_itr->high_bidder.value);
    if (bidrefunds_itr != bidrefunds.end()){
      bidrefunds.modify(bidrefunds_itr, same_payer, [&](auto & row){
        row.amount += namebids_itr->high_bid;
      });
    } else {
      bidrefunds.emplace(bidder, [&](auto & row){
        row.bidder = namebids_itr->high_bidder;
        row.amount = namebids_itr->high_bid;
      });
    }

    namebids.modify(namebids_itr, bidder, [&](auto & row){
      row.high_bidder = bidder;
      row.high_bid = bid;
      row.last_bid_time = current_time_point();
    });
  }
}

void namebidding::bidrefund(const name & bidder, const name & newname)
{
  _bidrefunds bidrefunds(get_self(), newname.value);
  auto bidrefunds_itr = bidrefunds.require_find(bidder.value, "refund not found");

  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
  std::make_tuple(
    get_self(),
    bidder,
    bidrefunds_itr->amount,
    std::string("refund bid on name ")+ newname.to_string()
  )).send();

  bidrefunds.erase(bidrefunds_itr);
}