#include <eosio.system.hpp>
#include "sections/native.cpp"

#include "delegate_bandwidth.cpp"
#include "finalizer_key.cpp"
#include "name_bidding.cpp"
#include "producer_pay.cpp"
#include "voting.cpp"


eosio_global_state native::get_default_parameters() {
  eosio_global_state dp;
  get_blockchain_parameters(dp);
  return dp;
}

native::native( name s, name code, datastream<const char*> ds )
  :native(s,code,ds),
  _voters(get_self(), get_self().value),
  _producers(get_self(), get_self().value),
  _finalizer_keys(get_self(), get_self().value),
  _finalizers(get_self(), get_self().value),
  _last_prop_finalizers(get_self(), get_self().value),
  _fin_key_id_generator(get_self(), get_self().value),
  _global(get_self(), get_self().value),
  _schedules(get_self(), get_self().value)
{
  _gstate  = _global.exists() ? _global.get() : get_default_parameters();
}

native::~native() {
  _global.set( _gstate, get_self() );
  _global2.set( _gstate2, get_self() );
  _global3.set( _gstate3, get_self() );
  _global4.set( _gstate4, get_self() );
}

void native::setram(uint64_t max_ram_size){
  require_auth(get_self());

  check( _gstate.max_ram_size < max_ram_size, "ram may only be increased");
  check( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
  check( max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved" );

  _gstate.max_ram_size = max_ram_size;
}

   void native::update_ram_supply() {
      auto cbt = eosio::current_block_time();

      if( cbt <= _gstate2.last_ram_increase ) return;

      if (_gstate2.new_ram_per_block != 0) {
         auto itr     = _rammarket.find(ramcore_symbol.raw());
         auto new_ram = (cbt.slot - _gstate2.last_ram_increase.slot) * _gstate2.new_ram_per_block;
         _gstate.max_ram_size += new_ram;

         /**
          *  Increase the amount of ram for sale based upon the change in max ram size.
          */
         _rammarket.modify(itr, same_payer, [&](auto& m) { m.base.balance.amount += new_ram; });
      }

      _gstate2.last_ram_increase = cbt;
   }

   void native::setramrate( uint16_t bytes_per_block ) {
      require_auth( get_self() );

      update_ram_supply(); // make sure all previous blocks are accounted at the old rate before updating the rate
      _gstate2.new_ram_per_block = bytes_per_block;
   }

   void native::channel_to_system_fees( const name& from, const asset& amount ) {
      token::transfer_action transfer_act{ token_account, { from, active_permission } };
      transfer_act.send( from, fees_account, amount, "transfer from " + from.to_string() + " to " + fees_account.to_string() );
   }

#ifdef SYSTEM_BLOCKCHAIN_PARAMETERS
   extern "C" [[eosio::wasm_import]] void set_parameters_packed(const void*, size_t);
#endif

   void native::setparams( const blockchain_parameters_t& params ) {
      require_auth( get_self() );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
#ifndef SYSTEM_BLOCKCHAIN_PARAMETERS
      set_blockchain_parameters( params );
#else
      constexpr size_t param_count = 18;
      // an upper bound on the serialized size
      char buf[1 + sizeof(params) + param_count];
      datastream<char*> stream(buf, sizeof(buf));

      stream << uint8_t(17);
      stream << uint8_t(0) << params.max_block_net_usage
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
   }

#ifdef SYSTEM_CONFIGURABLE_WASM_LIMITS

   // The limits on contract WebAssembly modules
   struct wasm_parameters
   {
    uint32_t max_mutable_global_bytes;
    uint32_t max_table_elements;
    uint32_t max_section_elements;
    uint32_t max_linear_memory_init;
    uint32_t max_func_local_bytes;
    uint32_t max_nested_structures;
    uint32_t max_symbol_bytes;
    uint32_t max_module_bytes;
    uint32_t max_code_bytes;
    uint32_t max_pages;
    uint32_t max_call_depth;
   };

   static constexpr wasm_parameters default_limits = {
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

   static constexpr wasm_parameters high_limits = {
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
       .max_call_depth = 1024
   };

   extern "C" [[eosio::wasm_import]] void set_wasm_parameters_packed( const void*, size_t );

   void set_wasm_parameters( const wasm_parameters& params )
   {
      char buf[sizeof(uint32_t) + sizeof(params)] = {};
      memcpy(buf + sizeof(uint32_t), &params, sizeof(params));
      set_wasm_parameters_packed( buf, sizeof(buf) );
   }

   void native::wasmcfg( const name& settings )
   {
      require_auth( get_self() );
      if( settings == "default"_n || settings == "low"_n )
      {
         set_wasm_parameters( default_limits );
      }
      else if( settings == "high"_n )
      {
         set_wasm_parameters( high_limits );
      }
      else
      {
         check(false, "Unknown configuration");
      }
   }

#endif

   void native::setpriv( const name& account, uint8_t ispriv ) {
      require_auth( get_self() );
      set_privileged( account, ispriv );
   }

   void native::setacctram( const name& account, const std::optional<int64_t>& ram_bytes ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t ram = 0;

      if( !ram_bytes ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::ram_managed ),
                "RAM of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         ram = ram_gift_bytes;
         if( ritr != userres.end() ) {
            ram += ritr->ram_bytes;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, false );
         });
      } else {
         check( *ram_bytes >= 0, "not allowed to set RAM limit to unlimited" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         }

         ram = *ram_bytes;
      }

      set_resource_limits( account, ram, current_net, current_cpu );
   }



   void native::activate( const eosio::checksum256& feature_digest ) {
      require_auth( get_self() );
      preactivate_feature( feature_digest );
   }

   void native::logsystemfee( const name& protocol, const asset& fee, const std::string& memo ) {
      require_auth( get_self() );
   }

   void native::rmvproducer( const name& producer ) {
      require_auth( get_self() );
      auto prod = _producers.find( producer.value );
      check( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, same_payer, [&](auto& p) {
            p.deactivate();
         });
   }

   void native::updtrevision( uint8_t revision ) {
      require_auth( get_self() );
      check( _gstate2.revision < 255, "can not increment revision" ); // prevent wrap around
      check( revision == _gstate2.revision + 1, "can only increment revision by one" );
      check( revision <= 1, // set upper bound to greatest revision supported in the code
             "specified revision is not yet supported by the code" );
      _gstate2.revision = revision;
   }

   void native::setschedule( const time_point_sec start_time, double continuous_rate )
   {
      require_auth( get_self() );

      check(continuous_rate >= 0, "continuous_rate can't be negative");
      check(continuous_rate <= 1, "continuous_rate can't be over 100%");

      auto itr = _schedules.find( start_time.sec_since_epoch() );

      if( itr == _schedules.end() ) {
         _schedules.emplace( get_self(), [&]( auto& s ) {
            s.start_time = start_time;
            s.continuous_rate = continuous_rate;
         });
      } else {
         _schedules.modify( itr, same_payer, [&]( auto& s ) {
            s.continuous_rate = continuous_rate;
         });
      }
   }

   void native::delschedule( const time_point_sec start_time )
   {
      require_auth( get_self() );

      auto itr = _schedules.require_find( start_time.sec_since_epoch(), "schedule not found" );
      _schedules.erase( itr );
   }

   void native::execschedule()
   {
      check(execute_next_schedule(), "no schedule to execute");
   }

   bool native::execute_next_schedule()
   {
      auto itr = _schedules.begin();
      if (itr == _schedules.end()) return false; // no schedules to execute

      if ( current_time_point().sec_since_epoch() >= itr->start_time.sec_since_epoch() ) {
         _gstate4.continuous_rate = itr->continuous_rate;
         _global4.set( _gstate4, get_self() );
         _schedules.erase( itr );
         return true;
      }
      return false;
   }









void native::add_to_blockinfo_table(const eosio::checksum256 & previous_block_id, const eosio::block_timestamp timestamp) const
{
  inline uint32_t block_height_from_id(const eosio::checksum256& block_id)
  {
    auto arr = block_id.extract_as_byte_array();
    // 32-bit block height is encoded in big endian as the sequence of bytes: arr[0], arr[1], arr[2], arr[3]
    return ((arr[0] << 0x18) | (arr[1] << 0x10) | (arr[2] << 0x08) | arr[3]);
  }

  const uint32_t new_block_height = block_height_from_id(previous_block_id) + 1;
  const auto new_block_timestamp = static_cast<eosio::time_point>(timestamp);

  block_info::block_info_table t(get_self(), 0);

  if (block_info::rolling_window_size > 0) {
    // Add new entry to blockinfo table for the new block.
    t.emplace(get_self(), [&](block_info::block_info_record& r) {
        r.block_height    = new_block_height;
        r.block_timestamp = new_block_timestamp;
    });
  }

  // Erase up to two entries that have fallen out of the rolling window.

  const uint32_t last_prunable_block_height =
    std::max(new_block_height, block_info::rolling_window_size) - block_info::rolling_window_size;

  int count = 2;
  for (auto itr = t.begin(), end = t.end();                                        //
      itr != end && itr->block_height <= last_prunable_block_height && 0 < count; //
      --count)                                                                    //
  {
    itr = t.erase(itr);
  }
}
