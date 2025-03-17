eosio::bls_g1 systemcore::to_binary(const std::string & finalizer_key)
{
  check(finalizer_key.compare(0, 7, "PUB_BLS") == 0, "finalizer key does not start with PUB_BLS");
  return eosio::decode_bls_public_key_to_g1(finalizer_key);
}

eosio::checksum256 systemcore::get_finalizer_key_hash(const std::string & finalizer_key)
{
  const auto fin_key_g1 = to_binary(finalizer_key);
  return get_finalizer_key_hash(fin_key_g1);
}

eosio::checksum256 systemcore::get_finalizer_key_hash(const eosio::bls_g1 & finalizer_key_binary)
{
  return eosio::sha256(finalizer_key_binary.data(), finalizer_key_binary.size());
}

bool systemcore::is_savanna_consensus(_lastpropfins & lastpropfins)
{
  return !get_last_proposed_finalizers(lastpropfins).empty();
}

std::vector<systemcore::finalizer_auth_info> & systemcore::get_last_proposed_finalizers(_lastpropfins & lastpropfins)
{
  if(!_last_prop_finalizers_cached.has_value()){
    const auto finalizers_itr = lastpropfins.begin();
    if(finalizers_itr == lastpropfins.end()){
      _last_prop_finalizers_cached = {};
    } else {
      _last_prop_finalizers_cached = finalizers_itr->last_proposed_finalizers;
    }
  }
  return *_last_prop_finalizers_cached;
}

void systemcore::set_proposed_finalizers(std::vector<systemcore::finalizer_auth_info> proposed_finalizers, _lastpropfins & lastpropfins)
{
  // Sort proposed_finalizers by finalizer key ID
  std::sort(proposed_finalizers.begin(), proposed_finalizers.end(), [](const systemcore::finalizer_auth_info & lhs, const systemcore::finalizer_auth_info & rhs){
    return lhs.key_id < rhs.key_id;
  });

  // Compare with last_proposed_finalizers to see if finalizers have changed.
  const auto & last_proposed_finalizers = get_last_proposed_finalizers(lastpropfins);
  if(proposed_finalizers == last_proposed_finalizers){
    // Finalizer policy has not changed. Do not proceed.
    return;
  }

  // Construct finalizer authorities
  std::vector<eosio::finalizer_authority> finalizer_authorities;
  finalizer_authorities.reserve(proposed_finalizers.size());
  for(const auto & k : proposed_finalizers){
    finalizer_authorities.emplace_back(k.fin_authority);
  }

  // Establish new finalizer policy
  eosio::finalizer_policy fin_policy {
    .threshold  = ( finalizer_authorities.size() * 2 ) / 3 + 1,
    .finalizers = std::move(finalizer_authorities)
  };

  // Call host function
  eosio::set_finalizers(std::move(fin_policy));

  // Store last proposed policy in both cache and DB table
  auto itr = lastpropfins.begin();
  if(itr == lastpropfins.end()){
    lastpropfins.emplace(get_self(), [&](auto & row){
      row.last_proposed_finalizers = proposed_finalizers;
    });
  } else {
    lastpropfins.modify(itr, same_payer, [&](auto & row){
      row.last_proposed_finalizers = proposed_finalizers;
    });
  }

  if (_last_prop_finalizers_cached.has_value()){
    std::swap(*_last_prop_finalizers_cached, proposed_finalizers);
  } else {
    _last_prop_finalizers_cached.emplace(std::move(proposed_finalizers));
  }
}

void systemcore::update_elected_producers(const block_timestamp & block_time)
{
  _global global(get_self(), get_self().value);
  auto _gstate = global.get();

  _gstate.last_producer_schedule_update = block_time;

  _producers producers(get_self(), get_self().value);
  auto idx = producers.get_index<name("prototalvote")>();

  using value_type = std::pair<eosio::producer_authority, uint16_t>;
  std::vector<value_type> top_producers;
  std::vector<finalizer_auth_info> proposed_finalizers;
  top_producers.reserve(21);
  proposed_finalizers.reserve(21);

  _finalizers finalizers(get_self(), get_self().value);
  for(auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it){

    auto finalizer = finalizers.find(it->owner.value);
    if(finalizer == finalizers.end()){
      // The producer is not in finalizers table, indicating it does not have an active registered finalizer key. Try next one.
      continue;
    }

    // This should never happen. Double check just in case
    if(finalizer->active_key_binary.empty()){
      continue;
    }

    proposed_finalizers.emplace_back(*finalizer);

    top_producers.emplace_back(
      eosio::producer_authority{
          .producer_name = it->owner,
          .authority = it->get_producer_authority()
      },
      it->location
    );
  }

  if(top_producers.size() == 0 || top_producers.size() < _gstate.last_producer_schedule_size){
    return;
  }

  // sort by producer name; // return lhs.second < rhs.second; // sort by location
  std::sort(top_producers.begin(), top_producers.end(), [](const value_type & lhs, const value_type & rhs){
    return lhs.first.producer_name < rhs.first.producer_name;
  });

  std::vector<eosio::producer_authority> new_producers;

  new_producers.reserve(top_producers.size());
  for(auto & item : top_producers)
    new_producers.push_back(std::move(item.first));

  if(set_proposed_producers(new_producers) >= 0){
    _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>(new_producers.size());
  }

  global.set(_gstate, get_self());

  _lastpropfins lastpropfins(get_self(), get_self().value);
  set_proposed_finalizers(std::move(proposed_finalizers), lastpropfins);
}

systemcore::_finalizers::const_iterator systemcore::get_finalizer_itr(const name & finalizer_name, _finalizers & finalizers) const
{
  auto finalizer_itr = finalizers.find(finalizer_name.value);
  check(finalizer_itr != finalizers.end(), "finalizer " + finalizer_name.to_string() + " has not registered any finalizer keys" );
  check(finalizer_itr->finalizer_key_count > 0, "finalizer " + finalizer_name.to_string() + "  must have at least one registered finalizer keys, has " + std::to_string(finalizer_itr->finalizer_key_count) );

  return finalizer_itr;
}

uint64_t systemcore::get_next_finalizer_key_id(_finkeyidgen & finkeyidgen)
{
  uint64_t next_id = 0;
  auto itr = finkeyidgen.begin();

  if(itr == finkeyidgen.end()){
    finkeyidgen.emplace(get_self(), [&](auto & row){
      row.next_finalizer_key_id = next_id;
    });
  } else {
    next_id = itr->next_finalizer_key_id  + 1;
    finkeyidgen.modify(itr, same_payer, [&](auto & row){
      row.next_finalizer_key_id = next_id;
    });
  }

  return next_id;
}

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