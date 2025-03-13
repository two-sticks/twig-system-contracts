void systemcore::on_a_lucky_block(name & producer, checksum256 & previous_block_id)
{
  _aluckynumber aluckynumber(get_self(), get_self().value);
  auto new_luck = aluckynumber.get();

  RandomnessProvider randomness_provider(new_luck.seed, new_luck.total_blocks + 1);
  new_luck.seed = randomness_provider.get_blend(previous_block_id, (uint64_t)producer.value);

  ++new_luck.total_blocks;
  uint32_t luck_roll = randomness_provider.get_rand(new_luck.odds * 16);
  if (luck_roll >= 16){
    ++new_luck.blocks_since;
  } else { // Hit!
    new_luck.blocks_since = 0;
    new_luck.producer = producer;
    if (new_luck.chunks_remaining > 0){
      --new_luck.chunks_remaining;
      eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, get_self(), eosio::name("onchunk"), std::make_tuple()).send();

    } else { // New Epoch
      new_luck.chunks_remaining = lucky_number_chunks;
      ++new_luck.epoch;
      eosio::action(eosio::permission_level{get_self(), eosio::name("active")}, bank_account, eosio::name("onepoch"), std::make_tuple(new_luck.epoch)).send();
    }
  }
  aluckynumber.set(new_luck, get_self());
}