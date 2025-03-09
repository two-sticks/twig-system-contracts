#pragma once

#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>
#include <eosio/privileged.hpp>

namespace eosioboot {

   using eosio::action_wrapper;
   using eosio::check;
   using eosio::checksum256;
   using eosio::ignore;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;

   struct permission_level_weight {
      permission_level permission;
      uint16_t weight;

      EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
   };

   struct key_weight {
      eosio::public_key key;
      uint16_t weight;

      EOSLIB_SERIALIZE(key_weight, (key)(weight))
   };

   struct wait_weight {
      uint32_t wait_sec;
      uint16_t weight;

      EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
   };

   struct authority {
      uint32_t threshold = 0;
      std::vector<key_weight> keys;
      std::vector<permission_level_weight> accounts;
      std::vector<wait_weight> waits;

      EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
   };

   CONTRACT boot : public eosio::contract {
      public:
         using eosio::contract::contract;

         [[eosio::action]] void newaccount(name creator, name name, ignore<authority> owner, ignore<authority> active){}
         [[eosio::action]] void updateauth(ignore<name> account, ignore<name> permission, ignore<name> parent, ignore<authority> auth){}
         [[eosio::action]] void deleteauth(ignore<name> account, ignore<name> permission){}
         [[eosio::action]] void linkauth(ignore<name> account, ignore<name> code, ignore<name> type, ignore<name> requirement){}
         [[eosio::action]] void unlinkauth(ignore<name> account, ignore<name> code, ignore<name> type){}
         [[eosio::action]] void canceldelay(ignore<permission_level> canceling_auth, ignore<checksum256> trx_id){}
         [[eosio::action]] void setcode(name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char> & code){}
         [[eosio::action]] void setabi(name account, const std::vector<char> & abi){}


         [[eosio::action]] void onerror(ignore<uint128_t> sender_id, ignore<std::vector<char>> sent_trx);
         [[eosio::action]] void activate(const eosio::checksum256 & feature_digest);
         [[eosio::action]] void reqactivated(const eosio::checksum256 & feature_digest);

         using newaccount_action = action_wrapper<"newaccount"_n, &boot::newaccount>;
         using updateauth_action = action_wrapper<"updateauth"_n, &boot::updateauth>;
         using deleteauth_action = action_wrapper<"deleteauth"_n, &boot::deleteauth>;
         using linkauth_action = action_wrapper<"linkauth"_n, &boot::linkauth>;
         using unlinkauth_action = action_wrapper<"unlinkauth"_n, &boot::unlinkauth>;
         using canceldelay_action = action_wrapper<"canceldelay"_n, &boot::canceldelay>;
         using setcode_action = action_wrapper<"setcode"_n, &boot::setcode>;
         using setabi_action = action_wrapper<"setabi"_n, &boot::setabi>;
         using activate_action = action_wrapper<"activate"_n, &boot::activate>;
         using reqactivated_action = action_wrapper<"reqactivated"_n, &boot::reqactivated>;
   };
}
