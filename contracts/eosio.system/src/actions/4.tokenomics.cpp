void systemcore::feedthebeast(checksum256 & seed)
{
  _aluckynumber aluckynumber_(get_self(), get_self().value);
  auto aluckynumber = aluckynumber_.get();

  RandomnessProvider randomness_provider(seed);
  aluckynumber.seed = randomness_provider.get_blend(aluckynumber.seed);

  aluckynumber_.set(aluckynumber, get_self());
}

void systemcore::onblock(ignore<block_header>)
{
  require_auth(get_self());
  _global global_(get_self(), get_self().value);
  auto global = global_.get();

  _aluckynumber aluckynumber_(get_self(), get_self().value);
  auto aluckynumber = aluckynumber_.get();

  block_timestamp timestamp;
  name producer;
  uint16_t confirmed;
  checksum256 previous_block_id;

  _ds >> timestamp >> producer >> confirmed >> previous_block_id;
  (void)confirmed;

  add_to_blockinfo_table(previous_block_id, timestamp);

  // Somewhere onBlock here, there needs to be the injection of the cycling ephemeral private_key & the commit-reveal validation check to prevent deterministic RNG prediction

  RandomnessProvider randomness_provider(aluckynumber.seed, aluckynumber.total_blocks + 1);
  aluckynumber.seed = randomness_provider.get_blend(previous_block_id, (uint64_t)producer.value);

  on_a_lucky_block(producer, aluckynumber, randomness_provider);
  process_rng_calls(aluckynumber, randomness_provider);

  // only update block producers once every minute, do extra work with finalizers & producers in the future ->>>
  if(timestamp.slot - global.last_producer_schedule_update.slot > blocks_per_minute){
    update_elected_producers(timestamp);
  }

  global_.set(global, get_self());
  aluckynumber_.set(aluckynumber, get_self());

  /* Namebids, daily trigger ->
  if((timestamp.slot - global.last_name_close.slot) > blocks_per_day){
    //eosio::action(eosio::permission_level{names_account, name("active")}, names_account, name("exec"), std::make_tuple().send();
  }
  */
}

void systemcore::onchunk()
{
  require_auth(get_self());

  _aluckynumber aluckynumber_(get_self(), get_self().value);
  auto aluckynumber = aluckynumber_.get();

  token::_accounts accounts(token_account, token_account.value);
  auto chunks_balance = accounts.require_find(chunks_account.value, "chunks_account_missing");

  asset chunks_tokens{chunks_balance->balance.find(core_symbol)->second, chunks_balance->balance.find(core_symbol)->first};

  // Issue tokens
  if (aluckynumber.epoch < 21){
    token::_supplies supplies_(token_account, token_account.value);
    auto supplies = supplies_.get();

    std::map<eosio::symbol, int64_t> tokens_to_issue;
    tokens_to_issue.emplace(core_symbol, (int64_t)(supplies.max_supply.find(core_symbol)->second * lucky_number_chunk_weight));

    chunks_tokens.amount += tokens_to_issue.find(core_symbol)->second;
    // Add in checks to not overmint from the max supply -> later
    eosio::action(permission_level{token_account, name("active")}, token_account, name("issue"),
      std::make_tuple(chunks_account, tokens_to_issue, (std::string)"A chunk has been found! Distributing rewards...")).send();
  }

  /// TEAM TOKENS ///
  asset team_tokens = chunks_tokens;
  team_tokens.amount = team_tokens.amount * lucky_number_team_weight;
  eosio::action(permission_level{chunks_account, name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, bank_account, (std::vector<asset>){team_tokens}, (std::string)"team_share")).send();

  /// LEADERBOARD TOKENS ///
  asset board_tokens = chunks_tokens;
  board_tokens.amount = board_tokens.amount * lucky_number_board_weight;
  eosio::action(permission_level{chunks_account, name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, board_account, (std::vector<asset>){board_tokens}, (std::string)"board_share")).send();

  /// PRODUCER TOKENS ///
  asset producer_tokens = chunks_tokens;
  producer_tokens.amount = producer_tokens.amount * lucky_number_producer_weight;
  eosio::action(permission_level{chunks_account, name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, bank_account, (std::vector<asset>){producer_tokens}, (std::string)"producer_share")).send();

  /// CHUNK -> WORLD TOKENS ///
  chunks_tokens.amount = chunks_tokens.amount * lucky_number_world_weight;
  eosio::action(permission_level{chunks_account, name("active")}, token_account, name("transfer"),
    std::make_tuple(chunks_account, world_account, (std::vector<asset>){chunks_tokens}, (std::string)"Refilling World...")).send();
}

