void systemcore::logsystemfee(const name & protocol, const asset & fee, const std::string & memo)
{
  require_auth(get_self());
}

void systemcore::feedthebeast(eosio::checksum256 & seed)
{
  require_auth(get_self());

  _aluckynumber aluckynumber(get_self(), get_self().value);
  auto new_luck = aluckynumber.get();

  RandomnessProvider randomness_provider(seed);
  new_luck.seed = randomness_provider.get_blend(new_luck.seed);

  aluckynumber.set(new_luck, get_self());
}

void systemcore::onblock(ignore<block_header>)
{
  require_auth(get_self());
  _global global(get_self(), get_self().value);
  auto _gstate = global.get();

  block_timestamp timestamp;
  name producer;
  uint16_t confirmed;
  checksum256 previous_block_id;

  _ds >> timestamp >> producer >> confirmed >> previous_block_id;
  (void)confirmed; // Only to suppress warning since confirmed is not used.

  // Add latest block information to blockinfo table.
  add_to_blockinfo_table(previous_block_id, timestamp);

  _producers producers(get_self(), get_self().value);
  auto prod = producers.find(producer.value);
  if (prod != producers.end()){
    _gstate.total_unpaid_blocks++;
    producers.modify(prod, same_payer, [&](auto & row){
      row.unpaid_blocks++;
    });
  }

  on_a_lucky_block(producer, previous_block_id);

  /// only update block producers once every minute, block_timestamp is in half seconds
  if(timestamp.slot - _gstate.last_producer_schedule_update.slot > blocks_per_minute){
    update_elected_producers(timestamp);

    if((timestamp.slot - _gstate.last_name_close.slot) > blocks_per_day){
      // Namebids, daily trigger ->
      /*
      name_bid_table bids(get_self(), get_self().value);
      auto idx = bids.get_index<"highbid"_n>();
      auto highest = idx.lower_bound( std::numeric_limits<uint64_t>::max()/2 );
      if( highest != idx.end() &&
          highest->high_bid > 0 &&
          (current_time_point() - highest->last_bid_time) > microseconds(useconds_per_day) &&
          _gstate.thresh_activated_stake_time > time_point() &&
          (current_time_point() - _gstate.thresh_activated_stake_time) > microseconds(14 * useconds_per_day)
      ) {
          _gstate.last_name_close = timestamp;

          // logging
          native::logsystemfee_action logsystemfee_act{ get_self(), { {get_self(), active_permission} } };
          logsystemfee_act.send( names_account, asset( highest->high_bid, core_symbol() ), "buy name" );

          idx.modify( highest, same_payer, [&]( auto& b ){
            b.high_bid = -b.high_bid;
          });
      }
      */
    }
  }

  global.set(_gstate, get_self());
}

void systemcore::onchunk()
{
  require_auth(get_self());

  _aluckynumber aluckynumber(get_self(), get_self().value);
  auto new_luck = aluckynumber.get();

  asset chunk_tokens = token::get_balance(token_account, chunk_account, core_symbol.code());

  // Issue tokens
  if (new_luck.epoch < 20){
    auto tokens_to_issue = token::get_max_supply(token_account, core_symbol.code());
    tokens_to_issue.amount = tokens_to_issue.amount * lucky_number_chunk_weight;
    chunk_tokens += tokens_to_issue;
    // Add in checks to not overmint from the max supply -> later
    eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, token_account, eosio::name("issue"),
      std::make_tuple(chunk_account, tokens_to_issue, (std::string)"A chunk has been found! Distributing rewards...")).send();
  }

  /// TEAM TOKENS ///
  asset team_tokens = chunk_tokens;
  team_tokens.amount = team_tokens.amount * lucky_number_team_weight;
  eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, token_account, eosio::name("transfer"),
    std::make_tuple(chunk_account, bank_account, team_tokens, (std::string)"team_share")).send();

  /// LEADERBOARD TOKENS ///
  asset board_tokens = chunk_tokens;
  board_tokens.amount = board_tokens.amount * lucky_number_board_weight;
  eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, token_account, eosio::name("transfer"),
    std::make_tuple(chunk_account, board_account, board_tokens, (std::string)"board_share")).send();

  /// PRODUCER TOKENS ///
  asset producer_tokens = chunk_tokens;
  producer_tokens.amount = producer_tokens.amount * lucky_number_producer_weight;
  eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, token_account, eosio::name("transfer"),
    std::make_tuple(chunk_account, bank_account, producer_tokens, (std::string)"producer_share")).send();

  /// CHUNK -> WORLD TOKENS ///
  chunk_tokens.amount = chunk_tokens.amount * lucky_number_world_weight;
  eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, token_account, eosio::name("transfer"),
    std::make_tuple(chunk_account, world_account, chunk_tokens, (std::string)"Refilling World...")).send();
}


    /*
    -> If EPOCH < 20, issue tokens ->>> lucky_number_chunk_weight * maximum_supply

    -> Issued tokens + tokens from fees ->>> All sent & gathered at eosio.chunks

    -> onChunk(), -> send all tokens
    -> from eosio.chunks
    -> to eosio.world w/ world weight

    -> remaining is split amongst team, (leader)board & producers
    -> team & producers -> portion gets sent to eosio.bank & vests; becomes available for claiming during the next epoch
    -> board -> portion gets sent to eosio.board & vests; winners are chosen & then it becomes available for claiming during the next epoch
    */


void systemcore::claimrewards(const name & owner)
{
  require_auth(owner);
  _global global(get_self(), get_self().value);
  auto _gstate = global.get();

  _producers producers(get_self(), get_self().value);
  auto prod = producers.require_find(owner.value, "producer not registered");
  check(prod->active(), "producer does not have an active key");

  const auto ct = current_time_point();

  check(ct - prod->last_claim_time > microseconds(useconds_per_day), "already claimed rewards within past day" );

  /*
  const asset token_supply = token::get_supply(token_account, core_symbol().code() );
  const asset token_max_supply = token::get_max_supply(token_account, core_symbol().code() );
  const asset token_balance = token::get_balance(token_account, get_self(), core_symbol().code() );
  const auto usecs_since_last_fill = (ct - _gstate.last_pervote_bucket_fill).count();

  if( usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > time_point() ) {
      double additional_inflation = (_gstate4.continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year);
      check( additional_inflation <= double(std::numeric_limits<int64_t>::max() - ((1ll << 10) - 1)),
            "overflow in calculating new tokens to be issued; inflation rate is too high" );
      int64_t new_tokens = (additional_inflation < 0.0) ? 0 : static_cast<int64_t>(additional_inflation);

      int64_t to_producers = (new_tokens * uint128_t(pay_factor_precision)) / _gstate4.inflation_pay_factor;
      int64_t to_savings = new_tokens - to_producers;
      int64_t to_per_block_pay = (to_producers * uint128_t(pay_factor_precision)) / _gstate4.votepay_factor;
      int64_t to_per_vote_pay  = to_producers - to_per_block_pay;

      if(new_tokens > 0){
        {
            // issue new tokens if circulating supply does not exceed max supply
            if ( token_supply.amount + new_tokens <= token_max_supply.amount ) {
              token::issue_action issue_act{ token_account, { {get_self(), active_permission} } };
              issue_act.send( get_self(), asset(new_tokens, core_symbol()), "issue tokens for producer pay and savings" );

            // use existing eosio token balance if circulating supply exceeds max supply
            } else {
              check( token_balance.amount >= new_tokens, "insufficient system token balance for claiming rewards");
            }
        }
        {
            token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
            if( to_savings > 0 ) {
              transfer_act.send( get_self(), saving_account, asset(to_savings, core_symbol()), "unallocated bucket" );
            }
            if( to_per_block_pay > 0 ) {
              transfer_act.send( get_self(), bpay_account, asset(to_per_block_pay, core_symbol()), "fund per-block bucket" );
            }
            if( to_per_vote_pay > 0 ) {
              transfer_act.send( get_self(), vpay_account, asset(to_per_vote_pay, core_symbol()), "fund per-vote bucket" );
            }
        }
      }

      _gstate.pervote_bucket          += to_per_vote_pay;
      _gstate.perblock_bucket         += to_per_block_pay;
      _gstate.last_pervote_bucket_fill = ct;
  }


  int64_t producer_per_block_pay = 0;
  if( _gstate.total_unpaid_blocks > 0 ) {
      producer_per_block_pay = (_gstate.perblock_bucket * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
  }

  double new_votepay_share = update_producer_votepay_share( prod2,
                                ct,
                                updated_after_threshold ? 0.0 : prod.total_votes,
                                true // reset votepay_share to zero after updating
                              );

  int64_t producer_per_vote_pay = 0;

  if( _gstate.total_producer_vote_weight > 0 ) {
    producer_per_vote_pay = int64_t((_gstate.pervote_bucket * prod.total_votes) / _gstate.total_producer_vote_weight);
  }

  _gstate.total_unpaid_blocks -= prod.unpaid_blocks;

  _producers.modify(prod, same_payer, [&](auto& p){
    p.last_claim_time = ct;
    p.unpaid_blocks = 0;
  });
  */

  global.set(_gstate, get_self());
}

