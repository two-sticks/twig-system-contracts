void systemcore::on_a_lucky_block(name & producer, _aluckynumber_s & aluckynumber, RandomnessProvider & randomness_provider)
{
  ++aluckynumber.total_blocks;
  uint32_t luck_roll = randomness_provider.get_rand(aluckynumber.odds * 16);
  if (luck_roll >= 16){
    ++aluckynumber.blocks_since;
  } else { // Hit!
    aluckynumber.blocks_since = 0;
    aluckynumber.producer = producer;
    if (aluckynumber.chunks_remaining > 0){
      --aluckynumber.chunks_remaining;
      eosio::action(eosio::permission_level{get_self(), name("active")}, get_self(), name("onchunk"), std::make_tuple()).send();

    } else { // New Epoch
      aluckynumber.chunks_remaining = lucky_number_chunks;
      ++aluckynumber.epoch;
      eosio::action(eosio::permission_level{bank_account, name("active")}, bank_account, name("onepoch"), std::make_tuple(aluckynumber.epoch)).send();
    }
  }
}

void systemcore::process_rng_calls(_aluckynumber_s & aluckynumber, RandomnessProvider & randomness_provider)
{
  _rngcalls rngcalls(get_self(), get_self().value);
  auto rngcalls_itr = rngcalls.begin();

  while (rngcalls_itr != rngcalls.end()){
    if (rngcalls_itr->active != 0){
      eosio::action(eosio::permission_level{rngcalls_itr->contract, eosio::name("active")}, rngcalls_itr->contract, rngcalls_itr->action,
        std::make_tuple(randomness_provider.get_blend(aluckynumber.seed, (uint64_t)rngcalls_itr->contract.value))).send();
    }
    ++rngcalls_itr;
  }
}