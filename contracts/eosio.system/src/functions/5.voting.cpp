int64_t systemcore::update_voting_power(const name & voter, const asset & total_update)
{
  _voters voters(get_self(), get_self().value);
  auto voter_itr = voters.find(voter.value);
  if(voter_itr == voters.end()){
    voter_itr = voters.emplace(voter, [&](auto & row){
      row.owner = voter;
      row.staked = total_update.amount;
    });
  } else {
    voters.modify(voter_itr, same_payer, [&](auto & row){
      row.staked += total_update.amount;
    });
  }

  check(0 <= voter_itr->staked, "stake for voting cannot be negative");

  if(voter_itr->producers.size() || voter_itr->proxy){
    update_votes(voter, voter_itr->proxy, voter_itr->producers, false);
  }
  return voter_itr->staked;
}
void systemcore::update_votes(const name & voter_name, const name & proxy, const std::vector<name> & producers, bool voting)
{
  _global global(get_self(), get_self().value);
  auto _gstate = global.get();

  if (proxy){
    check(producers.size() == 0, "cannot vote for producers and proxy at same time");
    check(voter_name != proxy, "cannot proxy to self");
  } else {
    check(producers.size() <= 30, "attempt to vote for too many producers");
    for(size_t i = 1; i < producers.size(); ++i){
      check(producers[i-1] < producers[i], "producer votes must be unique and sorted");
    }
  }
  _voters voters(get_self(), get_self().value);
  auto voter = voters.require_find(voter_name.value, "user must stake before they can vote");
  check(!proxy || !voter->is_proxy, "account registered as a proxy is not allowed to use a proxy");

  /*
  if(voter->last_vote_weight <= 0.0){
    _gstate.total_activated_stake += voter->staked;
  }

  double new_vote_weight = (double)voter->staked;
  if(voter->is_proxy){
    new_vote_weight += (double)voter->proxied_vote_weight;
  }

  std::map<name, std::pair<double, bool> > producer_deltas;
  if ( voter->last_vote_weight > 0 ) {
      if( voter->proxy ) {
        auto old_proxy = _voters.find( voter->proxy.value );
        check( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
        _voters.modify( old_proxy, same_payer, [&]( auto& vp ) {
              vp.proxied_vote_weight -= voter->last_vote_weight;
            });
        propagate_weight_change( *old_proxy );
      } else {
        for( const auto& p : voter->producers ) {
            auto& d = producer_deltas[p];
            d.first -= voter->last_vote_weight;
            d.second = false;
        }
      }
  }

  if( proxy ) {
      auto new_proxy = _voters.find( proxy.value );
      check( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
      check( !voting || new_proxy->is_proxy, "proxy not found" );
      if ( new_vote_weight >= 0 ) {
        _voters.modify( new_proxy, same_payer, [&]( auto& vp ) {
              vp.proxied_vote_weight += new_vote_weight;
            });
        propagate_weight_change( *new_proxy );
      }
  } else {
      if( new_vote_weight >= 0 ) {
        for( const auto& p : producers ) {
            auto& d = producer_deltas[p];
            d.first += new_vote_weight;
            d.second = true;
        }
      }
  }

  const auto ct = current_time_point();
  double delta_change_rate         = 0.0;
  double total_inactive_vpay_share = 0.0;
  for( const auto& pd : producer_deltas ) {
      auto pitr = _producers.find( pd.first.value );
      if( pitr != _producers.end() ) {
        if( voting && !pitr->active() && pd.second.second ) {
            check( false, ( "producer " + pitr->owner.to_string() + " is not currently registered" ).data() );
        }
        double init_total_votes = pitr->total_votes;
        _producers.modify( pitr, same_payer, [&]( auto& p ) {
            p.total_votes += pd.second.first;
            if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
              p.total_votes = 0;
            }
            _gstate.total_producer_vote_weight += pd.second.first;
            //check( p.total_votes >= 0, "something bad happened" );
        });
        auto prod2 = _producers2.find( pd.first.value );
        if( prod2 != _producers2.end() ) {
            const auto last_claim_plus_3days = pitr->last_claim_time + microseconds(3 * useconds_per_day);
            bool crossed_threshold       = (last_claim_plus_3days <= ct);
            bool updated_after_threshold = (last_claim_plus_3days <= prod2->last_votepay_share_update);
            // Note: updated_after_threshold implies cross_threshold

            double new_votepay_share = update_producer_votepay_share( prod2,
                                          ct,
                                          updated_after_threshold ? 0.0 : init_total_votes,
                                          crossed_threshold && !updated_after_threshold // only reset votepay_share once after threshold
                                      );

            if( !crossed_threshold ) {
              delta_change_rate += pd.second.first;
            } else if( !updated_after_threshold ) {
              total_inactive_vpay_share += new_votepay_share;
              delta_change_rate -= init_total_votes;
            }
        }
      } else {
        if( pd.second.second ) {
            check( false, ( "producer " + pd.first.to_string() + " is not registered" ).data() );
        }
      }
  }

  _voters.modify( voter, same_payer, [&]( auto& av ) {
      av.last_vote_weight = new_vote_weight;
      av.producers = producers;
      av.proxy     = proxy;
  });

  */
  global.set(_gstate, get_self());
}