void systemcore::feedthebeast(checksum256 & seed)
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
  checksum256 transaction_mroot;
  checksum256 action_mroot;

  _ds >> timestamp >> producer >> confirmed >> previous_block_id >> transaction_mroot >> action_mroot;
  (void)confirmed;

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
      //eosio::action(eosio::permission_level{names_account, name("active")}, names_account, name("exec"), std::make_tuple().send();
    }
  }

  global.set(_gstate, get_self());
}

void systemcore::onchunk()
{
  require_auth(get_self());

  _aluckynumber aluckynumber(get_self(), get_self().value);
  auto new_luck = aluckynumber.get();

  asset chunk_tokens = token::get_balance(token_account, chunks_account, core_symbol.code());

  // Issue tokens
  if (new_luck.epoch < 20){
    auto tokens_to_issue = token::get_max_supply(token_account, core_symbol.code());
    tokens_to_issue.amount = tokens_to_issue.amount * lucky_number_chunk_weight;
    chunk_tokens += tokens_to_issue;
    // Add in checks to not overmint from the max supply -> later
    eosio::action(permission_level{get_self(), name("active")}, token_account, name("issue"),
      std::make_tuple(chunks_account, tokens_to_issue, (std::string)"A chunk has been found! Distributing rewards...")).send();
  }

  /// TEAM TOKENS ///
  asset team_tokens = chunk_tokens;
  team_tokens.amount = team_tokens.amount * lucky_number_team_weight;
  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, bank_account, team_tokens, (std::string)"team_share")).send();

  /// LEADERBOARD TOKENS ///
  asset board_tokens = chunk_tokens;
  board_tokens.amount = board_tokens.amount * lucky_number_board_weight;
  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, board_account, board_tokens, (std::string)"board_share")).send();

  /// PRODUCER TOKENS ///
  asset producer_tokens = chunk_tokens;
  producer_tokens.amount = producer_tokens.amount * lucky_number_producer_weight;
  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, bank_account, producer_tokens, (std::string)"producer_share")).send();

  /// CHUNK -> WORLD TOKENS ///
  chunk_tokens.amount = chunk_tokens.amount * lucky_number_world_weight;
  eosio::action(permission_level{get_self(), name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, world_account, chunk_tokens, (std::string)"Refilling World...")).send();
}

