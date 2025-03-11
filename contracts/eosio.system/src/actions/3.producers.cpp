void systemcore::regproducer(const name & producer, const eosio::block_signing_authority & producer_authority, const std::string & url, const uint16_t & location)
{
  check(url.size() < 512, "url too long");
  const auto ct = current_time_point();

  eosio::public_key producer_key{};
  std::visit([&](auto&& auth ){
    check(auth.is_valid(), "invalid producer authority");
    if(auth.keys.size() == 1){
      producer_key = auth.keys[0].key;
    }
  }, producer_authority);

  _producers producers(get_self(), get_self().value);
  auto prod = producers.find(producer.value);

  if (prod != producers.end()){
    check(has_auth(get_self()) || has_auth(producer), "invalid producer authority"); // EOSIO has to set add the producer first, privatized production
    producers.modify(prod, producer, [&](auto & row){
      row.producer_key = producer_key;
      row.is_active = true;
      row.url = url;
      row.location = location;
      row.producer_authority.emplace(producer_authority);
      if (row.last_claim_time == time_point())
        row.last_claim_time = ct;
    });
  } else {
    producers.emplace(producer, [&](auto & row){
      require_auth(get_self());
      row.owner = producer;
      row.total_votes = 0;
      row.producer_key = producer_key;
      row.is_active = true;
      row.url = url;
      row.location = location;
      row.last_claim_time = ct;
      row.producer_authority.emplace(producer_authority);
    });
  }
}

void systemcore::unregprod(const name & producer)
{
  require_auth(producer);
  _producers producers(get_self(), get_self().value);
  auto producer_itr = producers.require_find(producer.value, "producer not found");

  producers.modify(producer_itr, same_payer, [&](auto & row){
    row.deactivate();
  });
}

void systemcore::regfinkey(const name & finalizer_name, const std::string & finalizer_key, const std::string & proof_of_possession)
{
  require_auth(finalizer_name);

  _producers producers(get_self(), get_self().value);
  auto producer = producers.require_find(finalizer_name.value, "this finalizer is not a registered producer");

  // Basic signature format check
  check(proof_of_possession.compare(0, 7, "SIG_BLS") == 0, "proof of possession signature does not start with SIG_BLS");

  // Convert to binary form. The validity will be checked during conversion.
  const auto fin_key_g1 = to_binary(finalizer_key);
  const auto pop_g2 = eosio::decode_bls_signature_to_g2(proof_of_possession);

  // Duplication check across all registered keys
  _finkeys finkeys(get_self(), get_self().value);
  const auto idx = finkeys.get_index<name("byfinkey")>();
  const auto hash = get_finalizer_key_hash(fin_key_g1);
  check(idx.find(hash) == idx.end(), "duplicate finalizer key: " + finalizer_key);

  // Proof of possession check
  check(eosio::bls_pop_verify(fin_key_g1, pop_g2), "proof of possession check failed");

  // Insert the finalizer key into finalyzer_keys table
  _finkeyidgen finkeyidgen(get_self(), get_self().value);
  const auto finkey_itr = finkeys.emplace(finalizer_name, [&](auto & row){
    row.id = get_next_finalizer_key_id(finkeyidgen);
    row.finalizer_name = finalizer_name;
    row.finalizer_key = finalizer_key;
    row.finalizer_key_binary = { fin_key_g1.begin(), fin_key_g1.end() };
  });

  // Update finalizers table
  _finalizers finalizers(get_self(), get_self().value);
  auto finalizer = finalizers.find(finalizer_name.value);
  if(finalizer == finalizers.end()){
      // This is the first time the finalizer registering a finalizer key, mark the key active
      finalizers.emplace(finalizer_name, [&](auto & row){
        row.finalizer_name = finalizer_name;
        row.active_key_id = finkey_itr->id;
        row.active_key_binary = finkey_itr->finalizer_key_binary;
        row.finalizer_key_count = 1;
      });
  } else {
      // Update finalizer_key_count
      finalizers.modify(finalizer, same_payer, [&](auto & row){
        ++row.finalizer_key_count;
      });
  }
}

void systemcore::actfinkey(const name & finalizer_name, const std::string & finalizer_key)
{
  require_auth(finalizer_name);

  _finalizers finalizers(get_self(), get_self().value);
  const auto finalizer = get_finalizer_itr(finalizer_name, finalizers);

  // Check the key is registered
  _finkeys finkeys(get_self(), get_self().value);
  const auto idx = finkeys.get_index<name("byfinkey")>();
  const auto hash = get_finalizer_key_hash(finalizer_key);
  const auto finkey_itr = idx.find(hash);
  check(finkey_itr != idx.end(), "finalizer key was not registered");

  // Check the key belongs to finalizer
  check(finkey_itr->finalizer_name == name(finalizer_name), "finalizer key was not registered by the finalizer");

  // Check if the finalizer key is not already active
  check(!finkey_itr->is_active(finalizer->active_key_id), "finalizer key was already active");

  const auto active_key_id = finalizer->active_key_id;

  // Mark the finalizer key as active by updating finalizer's information in finalizers table
  finalizers.modify(finalizer, same_payer, [&](auto & row){
    row.active_key_id = finkey_itr->id;
    row.active_key_binary = finkey_itr->finalizer_key_binary;
  });

  _lastpropfins lastpropfins(get_self(), get_self().value);
  const auto & last_proposed_finalizers = get_last_proposed_finalizers(lastpropfins);
  if(last_proposed_finalizers.empty()){
    // No Savannah
    return;
  }

  // Search last_proposed_finalizers for active_key_id
  auto itr = std::lower_bound(last_proposed_finalizers.begin(), last_proposed_finalizers.end(), active_key_id, [](const systemcore::finalizer_auth_info & key, uint64_t id) {
    return key.key_id < id;
  });

  // If active_key_id is in last_proposed_finalizers, it means the finalizer is
  // active. Replace the existing entry in last_proposed_finalizers with
  // the information of finalizer_key just activated and call set_proposed_finalizers immediatelys
  if(itr != last_proposed_finalizers.end() && itr->key_id == active_key_id){
    auto proposed_finalizers = last_proposed_finalizers;
    auto & matching_entry = proposed_finalizers[itr - last_proposed_finalizers.begin()];

    matching_entry.key_id = finkey_itr->id;
    matching_entry.fin_authority.public_key = finkey_itr->finalizer_key_binary;

    set_proposed_finalizers(std::move(proposed_finalizers), lastpropfins);
  }
}

void systemcore::delfinkey(const name & finalizer_name, const std::string & finalizer_key)
{
  require_auth(finalizer_name);

  _finalizers finalizers(get_self(), get_self().value);
  const auto finalizer = get_finalizer_itr(finalizer_name, finalizers);

  // Check the key is registered
  _finkeys finkeys(get_self(), get_self().value);
  auto idx = finkeys.get_index<name("byfinkey")>();
  auto hash = get_finalizer_key_hash(finalizer_key);
  auto finkey_itr = idx.find(hash);
  check(finkey_itr != idx.end(), "finalizer key was not registered");

  // Check the key belongs to the finalizer
  check(finkey_itr->finalizer_name == name(finalizer_name), "finalizer key " + finalizer_key + " was not registered by the finalizer " + finalizer_name.to_string());

  if(finkey_itr->is_active(finalizer->active_key_id)){
    check( finalizer->finalizer_key_count == 1, "cannot delete an active key unless it is the last registered finalizer key, has " + std::to_string(finalizer->finalizer_key_count) + " keys");
  }

  // Update finalizers table
  if(finalizer->finalizer_key_count == 1){
    finalizers.erase(finalizer);
  } else {
    finalizers.modify(finalizer, same_payer, [&](auto & row){
      --row.finalizer_key_count;
    });
  }

  idx.erase(finkey_itr);
}

void systemcore::switchtosvnn()
{
  require_auth(get_self());
  _global global(get_self(), get_self().value);
  auto _gstate = global.get();

  _lastpropfins lastpropfins(get_self(), get_self().value);
  check(!is_savanna_consensus(lastpropfins), "switchtosvnn can be run only once");

  std::vector<systemcore::finalizer_auth_info> proposed_finalizers;
  proposed_finalizers.reserve(_gstate.last_producer_schedule_size);

  _finalizers finalizers(get_self(), get_self().value);
  _producers producers(get_self(), get_self().value);
  auto idx = producers.get_index<name("prototalvote")>();
  for(auto it = idx.cbegin(); it != idx.cend() && proposed_finalizers.size() < _gstate.last_producer_schedule_size && 0 < it->total_votes && it->active(); ++it){
      auto finalizer = finalizers.find(it->owner.value);
      if(finalizer == finalizers.end()){
        // The producer is not in finalizers table, indicating it does not have an, active registered finalizer key. Try next one.
        continue;
      }
      // This should never happen. Double check the finalizer has an active key just in case
      if(finalizer->active_key_binary.empty()){
        continue;
      }
      proposed_finalizers.emplace_back(*finalizer);
  }

  check(proposed_finalizers.size() == _gstate.last_producer_schedule_size,
        "not enough top producers have registered finalizer keys, has " + std::to_string(proposed_finalizers.size()) + ", require " + std::to_string(_gstate.last_producer_schedule_size) );

  set_proposed_finalizers(std::move(proposed_finalizers), lastpropfins);
  check(is_savanna_consensus(lastpropfins), "switching to Savanna failed");
}