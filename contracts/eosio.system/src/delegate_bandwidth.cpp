


   std::pair<int64_t, int64_t> get_b1_vesting_info() {
      const int64_t base_time = 1527811200; /// Friday, June 1, 2018 12:00:00 AM UTC
      const int64_t current_time = 1638921540; /// Tuesday, December 7, 2021 11:59:00 PM UTC
      const int64_t total_vesting = 100'000'000'0000ll;
      const int64_t vested = int64_t(total_vesting * double(current_time - base_time) / (10*seconds_per_year) );
      return { total_vesting, vested };
   }


   void validate_b1_vesting( int64_t new_stake, asset stake_change ) {
      const auto [total_vesting, vested] = get_b1_vesting_info();
      auto unvestable = total_vesting - vested;

      auto hasAlreadyUnvested = new_stake < unvestable
            && stake_change.amount < 0
            && new_stake + std::abs(stake_change.amount) < unvestable;
      if(hasAlreadyUnvested) return;

      check( new_stake >= unvestable, "b1 can only claim what has already vested" );
   }


   void system::update_user_resources( const name from, const name receiver, const asset stake_net_delta, const asset stake_cpu_delta )
   {
      user_resources_table   totals_tbl( get_self(), receiver.value );
      auto tot_itr = totals_tbl.find( receiver.value );
      if( tot_itr ==  totals_tbl.end() ) {
         tot_itr = totals_tbl.emplace( from, [&]( auto& tot ) {
               tot.owner = receiver;
               tot.net_weight    = stake_net_delta;
               tot.cpu_weight    = stake_cpu_delta;
            });
      } else {
         totals_tbl.modify( tot_itr, from == receiver ? from : same_payer, [&]( auto& tot ) {
               tot.net_weight    += stake_net_delta;
               tot.cpu_weight    += stake_cpu_delta;
            });
      }
      check( 0 <= tot_itr->net_weight.amount, "insufficient staked total net bandwidth" );
      check( 0 <= tot_itr->cpu_weight.amount, "insufficient staked total cpu bandwidth" );

      {
         bool ram_managed = false;
         bool net_managed = false;
         bool cpu_managed = false;

         auto voter_itr = _voters.find( receiver.value );
         if( voter_itr != _voters.end() ) {
            ram_managed = has_field( voter_itr->flags1, voter_info::flags1_fields::ram_managed );
            net_managed = has_field( voter_itr->flags1, voter_info::flags1_fields::net_managed );
            cpu_managed = has_field( voter_itr->flags1, voter_info::flags1_fields::cpu_managed );
         }

         if( !(net_managed && cpu_managed) ) {
            int64_t ram_bytes, net, cpu;
            get_resource_limits( receiver, ram_bytes, net, cpu );

            set_resource_limits( receiver,
                                 ram_managed ? ram_bytes : std::max( tot_itr->ram_bytes + ram_gift_bytes, ram_bytes ),
                                 net_managed ? net : tot_itr->net_weight.amount,
                                 cpu_managed ? cpu : tot_itr->cpu_weight.amount );
         }
      }

      if ( tot_itr->is_empty() ) {
         totals_tbl.erase( tot_itr );
      } // tot_itr can be invalid, should go out of scope
   }

   int64_t system::update_voting_power( const name& voter, const asset& total_update )
   {
      auto voter_itr = _voters.find( voter.value );
      if( voter_itr == _voters.end() ) {
         voter_itr = _voters.emplace( voter, [&]( auto& v ) {
            v.owner  = voter;
            v.staked = total_update.amount;
         });
      } else {
         _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
            v.staked += total_update.amount;
         });
      }

      check( 0 <= voter_itr->staked, "stake for voting cannot be negative" );

      if( voter_itr->producers.size() || voter_itr->proxy ) {
         update_votes( voter, voter_itr->proxy, voter_itr->producers, false );
      }
      return voter_itr->staked;
   }


   void system::refund( const name& owner ) {
      require_auth( owner );

      refunds_table refunds_tbl( get_self(), owner.value );
      auto req = refunds_tbl.find( owner.value );
      check( req != refunds_tbl.end(), "refund request not found" );
      check( req->request_time + seconds(refund_delay_sec) <= current_time_point(),
             "refund is not available yet" );
      token::transfer_action transfer_act{ token_account, { {stake_account, active_permission}, {req->owner, active_permission} } };
      transfer_act.send( stake_account, req->owner, req->net_amount + req->cpu_amount, "unstake" );
      refunds_tbl.erase( req );
   }

   void system::unvest(const name account, const asset unvest_net_quantity, const asset unvest_cpu_quantity)
   {
      require_auth( get_self() );

      check( account == "b1"_n, "only b1 account can unvest");

      check( unvest_cpu_quantity.amount >= 0, "must unvest a positive amount" );
      check( unvest_net_quantity.amount >= 0, "must unvest a positive amount" );

      const auto [total_vesting, vested] = get_b1_vesting_info();
      const asset unvesting = unvest_net_quantity + unvest_cpu_quantity;
      check( unvesting.amount <= total_vesting - vested , "can only unvest what is not already vested");

      // reduce staked from account
      update_voting_power( account, -unvesting );
      update_user_resources( account, account, -unvest_net_quantity, -unvest_cpu_quantity );
      vote_stake_updater( account );

      // transfer unvested tokens to `eosio`
      token::transfer_action transfer_act{ token_account, { {stake_account, active_permission} } };
      transfer_act.send( stake_account, get_self(), unvesting, "unvest" );

      // retire unvested tokens
      token::retire_action retire_act{ token_account, { {"eosio"_n, active_permission} } };
      retire_act.send( unvesting, "unvest" );
   }
