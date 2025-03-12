uint32_t systemcore::block_height_from_id(const eosio::checksum256 & block_id) const
{
  auto arr = block_id.extract_as_byte_array();
  // 32-bit block height is encoded in big endian as the sequence of bytes: arr[0], arr[1], arr[2], arr[3]
  return ((arr[0] << 0x18) | (arr[1] << 0x10) | (arr[2] << 0x08) | arr[3]);
}

void systemcore::add_to_blockinfo_table(const eosio::checksum256 & previous_block_id, const eosio::block_timestamp timestamp) const
{
  const uint32_t new_block_height = block_height_from_id(previous_block_id) + 1;
  const auto new_block_timestamp = static_cast<eosio::time_point>(timestamp);

  _blockinfo blockinfo(get_self(), 0);

  if (rolling_window_size > 0) {
    blockinfo.emplace(get_self(), [&](auto & row){
      row.block_height = new_block_height;
      row.block_timestamp = new_block_timestamp;
    });
  }

  const uint32_t last_prunable_block_height = std::max(new_block_height, rolling_window_size) - rolling_window_size;

  int count = 2;
  for (auto itr = blockinfo.begin(), end = blockinfo.end(); itr != end && itr->block_height <= last_prunable_block_height && 0 < count; --count)                                                                 //
  {
    itr = blockinfo.erase(itr);
  }
}

systemcore::latest_block_batch_info_result systemcore::get_latest_block_batch_info(uint32_t batch_start_height_offset, uint32_t batch_size, name system_account_name)
{
  latest_block_batch_info_result result;

  if (batch_size == 0) {
    result.error_code = latest_block_batch_info_result::invalid_input;
    return result;
  }

  _blockinfo blockinfo(get_self(), 0);

  // Find information on latest block recorded in the blockinfo table.
  if (blockinfo.cbegin() == blockinfo.cend()) {
    // The blockinfo table is empty.
    result.error_code = latest_block_batch_info_result::insufficient_data;
    return result;
  }

  auto latest_block_info_itr = --blockinfo.cend();

  if (latest_block_info_itr->version != 0) {
    // Compiled code for this function within the calling contract has not been updated to support new version of the blockinfo table.
    result.error_code = latest_block_batch_info_result::unsupported_version;
    return result;
  }

  uint32_t latest_block_batch_end_height = latest_block_info_itr->block_height;

  if (latest_block_batch_end_height < batch_start_height_offset) {
    // Caller asking for a block batch that has not even begun to be recorded yet.
    result.error_code = latest_block_batch_info_result::insufficient_data;
    return result;
  }

  // Calculate height for the starting block of the latest block batch.
  uint32_t latest_block_batch_start_height = latest_block_batch_end_height - ((latest_block_batch_end_height - batch_start_height_offset) % batch_size);

  // Note: 1 <= (latest_block_batch_end_height - latest_block_batch_start_height + 1) <= batch_size

  if (latest_block_batch_start_height == latest_block_batch_end_height) {
    // When batch_size == 1, this function effectively simplifies to just returning the info of the latest recorded
    // block. In that case, the start block and the end block of the batch are the same and there is no need for
    // another lookup. So shortcut the rest of the process and return a successful result immediately.
    result.result.emplace(block_batch_info{
      .batch_start_height          = latest_block_batch_start_height,
      .batch_start_timestamp       = latest_block_info_itr->block_timestamp,
      .batch_current_end_height    = latest_block_batch_end_height,
      .batch_current_end_timestamp = latest_block_info_itr->block_timestamp,
    });
    return result;
  }

  // Find information on start block of the latest block batch recorded in the blockinfo table.
  auto start_block_info_itr = blockinfo.find(latest_block_batch_start_height);
  if (start_block_info_itr == blockinfo.cend() || start_block_info_itr->block_height != latest_block_batch_start_height) {
    // Record for information on start block of the latest block batch could not be found in blockinfo table.
    // This is either because of:
    //    * a gap in recording info due to a failed onblock action;
    //    * a requested start block that was processed by onblock prior to deployment of the system contract code
    //    introducing the blockinfo table;
    //    * or, most likely, because the record for the requested start block was pruned from the blockinfo table as
    //    it fell out of the rolling window.
    result.error_code = latest_block_batch_info_result::insufficient_data;
    return result;
  }

  if (start_block_info_itr->version != 0) {
    // Compiled code for this function within the calling contract has not been updated to support new version of the blockinfo table.
    result.error_code = latest_block_batch_info_result::unsupported_version;
    return result;
  }

  // Successfully return block_batch_info for the found latest block batch in its current state.
  result.result.emplace(block_batch_info{
    .batch_start_height = latest_block_batch_start_height,
    .batch_start_timestamp = start_block_info_itr->block_timestamp,
    .batch_current_end_height = latest_block_batch_end_height,
    .batch_current_end_timestamp = latest_block_info_itr->block_timestamp,
  });
  return result;
}

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
      //onluckyepoch();
    }
  }
  aluckynumber.set(new_luck, get_self());
}