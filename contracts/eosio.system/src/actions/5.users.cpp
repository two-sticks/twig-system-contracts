void systemcore::voteproducer(const name & voter_name, const name & proxy, const std::vector<name> & producers)
{
  if (voter_name == name("b1")){
    require_auth(get_self());
  } else {
    require_auth(voter_name);
  }

  //vote_stake_updater(voter_name);
  update_votes(voter_name, proxy, producers, true);
}

void systemcore::voteupdate(const name & voter_name)
{
  _voters voters(get_self(), get_self().value);
  auto voter = voters.require_find(voter_name.value, "no voter found");

  int64_t new_staked = 0;

  /*
  updaterex(voter_name);

  // get rex bal
  auto rex_itr = _rexbalance.find( voter_name.value );
  if( rex_itr != _rexbalance.end() && rex_itr->rex_balance.amount > 0 ) {
      new_staked += rex_itr->vote_stake.amount;
  }
  del_bandwidth_table     del_tbl( get_self(), voter_name.value );

  auto del_itr = del_tbl.begin();
  while(del_itr != del_tbl.end()) {
      new_staked += del_itr->net_weight.amount + del_itr->cpu_weight.amount;
      del_itr++;
  }

  if( voter->staked != new_staked){
      // check if staked and new_staked are different and only
      _voters.modify( voter, same_payer, [&]( auto& av ) {
        av.staked = new_staked;
      });
  }

  update_votes(voter_name, voter->proxy, voter->producers, true);
  */
}

void systemcore::regproxy(const name & proxy, bool isproxy)
{
  require_auth(proxy);

  _voters voters(get_self(), get_self().value);
  auto pitr = voters.find(proxy.value);
  if (pitr != voters.end()){
    check(isproxy != pitr->is_proxy, "action has no effect");
    check(!isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy");
    voters.modify(pitr, same_payer, [&](auto & row){
      row.is_proxy = isproxy;
    });
  } else {
    voters.emplace(proxy, [&](auto & row){
      row.owner  = proxy;
      row.is_proxy = isproxy;
    });
  }
}

void systemcore::bidname(const name & bidder, const name & newname, const asset & bid)
{
    /*
    require_auth( bidder );
    check( newname.suffix() == newname, "you can only bid on top-level suffix" );

    check( (bool)newname, "the empty name is not a valid account name to bid on" );
    check( (newname.value & 0xFull) == 0, "13 character names are not valid account names to bid on" );
    check( (newname.value & 0x1F0ull) == 0, "accounts with 12 character names and no dots can be created without bidding required" );
    check( !is_account( newname ), "account already exists" );
    check( bid.symbol == core_symbol(), "asset must be system token" );
    check( bid.amount > 0, "insufficient bid" );
    token::transfer_action transfer_act{ token_account, { {bidder, active_permission} } };
    transfer_act.send( bidder, names_account, bid, std::string("bid name ")+ newname.to_string() );
    name_bid_table bids(get_self(), get_self().value);
    print( name{bidder}, " bid ", bid, " on ", name{newname}, "\n" );
    auto current = bids.find( newname.value );
    if( current == bids.end() ) {
        bids.emplace( bidder, [&]( auto& b ) {
          b.newname = newname;
          b.high_bidder = bidder;
          b.high_bid = bid.amount;
          b.last_bid_time = current_time_point();
        });
    } else {
        check( current->high_bid > 0, "this auction has already closed" );
        check( bid.amount - current->high_bid > (current->high_bid / 10), "must increase bid by 10%" );
        check( current->high_bidder != bidder, "account is already highest bidder" );

        bid_refund_table refunds_table(get_self(), newname.value);

        auto it = refunds_table.find( current->high_bidder.value );
        if ( it != refunds_table.end() ) {
          refunds_table.modify( it, same_payer, [&](auto& r) {
                r.amount += asset( current->high_bid, core_symbol() );
              });
        } else {
          refunds_table.emplace( bidder, [&](auto& r) {
                r.bidder = current->high_bidder;
                r.amount = asset( current->high_bid, core_symbol() );
              });
        }

        bids.modify( current, bidder, [&]( auto& b ) {
          b.high_bidder = bidder;
          b.high_bid = bid.amount;
          b.last_bid_time = current_time_point();
        });
    }
    */
}

void systemcore::bidrefund(const name & bidder, const name & newname)
{
  /*
    bid_refund_table refunds_table(get_self(), newname.value);
    auto it = refunds_table.find( bidder.value );
    check( it != refunds_table.end(), "refund not found" );

    token::transfer_action transfer_act{ token_account, { {names_account, active_permission}, {bidder, active_permission} } };
    transfer_act.send( names_account, bidder, asset(it->amount), std::string("refund bid on name ")+(name{newname}).to_string() );
    refunds_table.erase( it );
  */
}