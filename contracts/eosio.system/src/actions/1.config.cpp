void systemcore::init(bool destruct, const std::string & memo)
{
  require_auth(get_self());
  _chainparams chainparams_(get_self(), get_self().value);
  _global global_(get_self(), get_self().value);
  _aluckynumber aluckynumber_(get_self(), get_self().value);
  if (destruct){
    chainparams_.remove();
    global_.remove();
    aluckynumber_.remove();
  } else {
    _chainparams_s dp;
    get_blockchain_parameters(dp);

    _global_s global;
    chainparams_.set(dp, get_self());
    global_.set(global, get_self());
    aluckynumber_.set(_aluckynumber_s{.producer = name("eosio")}, get_self());
  }
  //auto system_token_supply = token::get_supply(token_account, core_symbol.code());
  //check(system_token_supply.symbol == core_symbol, "specified core symbol does not exist (precision mismatch)");
}

#ifdef SYSTEM_BLOCKCHAIN_PARAMETERS
  extern "C" [[eosio::wasm_import]] void set_parameters_packed(const void*, size_t);
#endif

void systemcore::setparams(const blockchain_parameters_t & params)
{
  require_auth(get_self());
  _chainparams chainparams_(get_self(), get_self().value);
  auto chainparams = chainparams_.get();

  (eosio::blockchain_parameters&)(chainparams) = params;
  check(3 <= chainparams.max_authority_depth, "max_authority_depth should be at least 3" );

#ifndef SYSTEM_BLOCKCHAIN_PARAMETERS
  set_blockchain_parameters(params);
#else
  constexpr size_t param_count = 18;
  // an upper bound on the serialized size
  char buf[1 + sizeof(params) + param_count];
  datastream<char*> stream(buf, sizeof(buf));

  stream  << uint8_t(17);
  stream  << uint8_t(0) << params.max_block_net_usage
          << uint8_t(1) << params.target_block_net_usage_pct
          << uint8_t(2) << params.max_transaction_net_usage
          << uint8_t(3) << params.base_per_transaction_net_usage
          << uint8_t(4) << params.net_usage_leeway
          << uint8_t(5) << params.context_free_discount_net_usage_num
          << uint8_t(6) << params.context_free_discount_net_usage_den

          << uint8_t(7) << params.max_block_cpu_usage
          << uint8_t(8) << params.target_block_cpu_usage_pct
          << uint8_t(9) << params.max_transaction_cpu_usage
          << uint8_t(10) << params.min_transaction_cpu_usage

          << uint8_t(11) << params.max_transaction_lifetime
          << uint8_t(12) << params.deferred_trx_expiration_window
          << uint8_t(13) << params.max_transaction_delay
          << uint8_t(14) << params.max_inline_action_size
          << uint8_t(15) << params.max_inline_action_depth
          << uint8_t(16) << params.max_authority_depth;
  if(params.max_action_return_value_size)
  {
    stream << uint8_t(17) << params.max_action_return_value_size.value();
    ++buf[0];
  }
  set_parameters_packed(buf, stream.tellp());
#endif
  chainparams_.set(chainparams, get_self());
}

static constexpr systemcore::wasm_parameters default_wasm_limits = {
  .max_mutable_global_bytes = 8192,
  .max_table_elements = 8192,
  .max_section_elements = 65536,
  .max_linear_memory_init = 512*1024,
  .max_func_local_bytes = 65536,
  .max_nested_structures = 65536,
  .max_symbol_bytes = 8192,
  .max_module_bytes = 100*1024*1024,
  .max_code_bytes = 100*1024*1024,
  .max_pages = 1040,
  .max_call_depth = 501
};

extern "C" [[eosio::wasm_import]] void set_wasm_parameters_packed(const void*, size_t);

void systemcore::wasmcfg(const name & settings)
{
  require_auth(get_self());
  char buf[sizeof(uint32_t) + sizeof(default_wasm_limits)] = {};
  memcpy(buf + sizeof(uint32_t), &default_wasm_limits, sizeof(default_wasm_limits));
  set_wasm_parameters_packed(buf, sizeof(buf));
}

void systemcore::activate(const checksum256 & feature_digest)
{
  require_auth(get_self());
  preactivate_feature(feature_digest);
}


