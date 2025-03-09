#include <eosio.system.hpp>
#include "sections/0.native.cpp"
#include "sections/1.config.cpp"
#include "sections/2.admin.cpp"
#include "sections/3.user.cpp"

#include "delegate_bandwidth.cpp"
#include "finalizer_key.cpp"
#include "producer_pay.cpp"
#include "voting.cpp"

   void system::channel_to_system_fees( const name& from, const asset& amount ) {
      token::transfer_action transfer_act{ token_account, { from, active_permission } };
      transfer_act.send( from, fees_account, amount, "transfer from " + from.to_string() + " to " + fees_account.to_string() );
   }

   void system::setschedule( const time_point_sec start_time, double continuous_rate )
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

   void system::delschedule( const time_point_sec start_time )
   {
      require_auth( get_self() );

      auto itr = _schedules.require_find( start_time.sec_since_epoch(), "schedule not found" );
      _schedules.erase( itr );
   }

   void system::execschedule()
   {
      check(execute_next_schedule(), "no schedule to execute");
   }

   bool system::execute_next_schedule()
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









void system::add_to_blockinfo_table(const eosio::checksum256 & previous_block_id, const eosio::block_timestamp timestamp) const
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
