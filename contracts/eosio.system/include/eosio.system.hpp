#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <limits>
#include <optional>
#include <set>

#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/dispatcher.hpp>
#include <eosio/eosio.hpp>

#include <eosio/permission.hpp>
#include <eosio/privileged.hpp>
#include <eosio/producer_schedule.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/serialize.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/instant_finality.hpp>
#include <eosio/transaction.hpp>

#include <eosio.token.hpp>

using namespace eosio;
CONTRACT systemcore : public eosio::contract {
  public:
    using eosio::contract::contract;

// STATIC
    static constexpr symbol core_symbol = symbol(symbol_code("TWIG"), 8);
    static constexpr symbol ram_symbol = symbol(symbol_code("RAM"), 0);

    static constexpr eosio::name active_permission{"active"_n};
    static constexpr eosio::name token_account{"eosio.token"_n};
    static constexpr eosio::name stake_account{"eosio.stake"_n};
    static constexpr eosio::name bpay_account{"eosio.bpay"_n};
    static constexpr eosio::name names_account{"eosio.names"_n};
    static constexpr eosio::name saving_account{"eosio.saving"_n};
    static constexpr eosio::name fees_account{"eosio.fees"_n};
    static constexpr eosio::name null_account{"eosio.null"_n};

    static constexpr uint32_t seconds_per_year = 52 * 7 * 24 * 3600;
    static constexpr uint32_t seconds_per_day = 24 * 3600;
    static constexpr uint32_t seconds_per_hour = 3600;
    static constexpr int64_t  useconds_per_year = int64_t(seconds_per_year) * 1000'000ll;
    static constexpr int64_t  useconds_per_day = int64_t(seconds_per_day) * 1000'000ll;
    static constexpr int64_t  useconds_per_hour = int64_t(seconds_per_hour) * 1000'000ll;
    static constexpr uint32_t blocks_per_day = seconds_per_day;
    static constexpr uint32_t blocks_per_minute = blocks_per_day / 24 / 60;

    static constexpr int64_t  inflation_precision = 100; // 2 decimals
    static constexpr int64_t  default_annual_rate = 500; // 5% annual rate

    static constexpr int64_t user_ram_limit = 1'000'000'000ll; // 1GB, just a big number

    static constexpr uint32_t rolling_window_size = 10;

// TEMPLATES
    template<typename E, typename F>
    static inline auto has_field( F flags, E field )
    -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, bool>
    {
      return ( (flags & static_cast<F>(field)) != 0 );
    }

    template<typename E, typename F>
    static inline auto set_field( F flags, E field, bool value = true )
    -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, F >
    {
      if( value )
          return ( flags | static_cast<F>(field) );
      else
          return ( flags & ~static_cast<F>(field) );
    }

// STRUCTS //
    #ifdef SYSTEM_BLOCKCHAIN_PARAMETERS
      struct blockchain_parameters_v1 : eosio::blockchain_parameters
      {
        eosio::binary_extension<uint32_t> max_action_return_value_size;
        EOSLIB_SERIALIZE_DERIVED(blockchain_parameters_v1, eosio::blockchain_parameters, (max_action_return_value_size))
      };
      using blockchain_parameters_t = blockchain_parameters_v1;
    #else
      using blockchain_parameters_t = eosio::blockchain_parameters;
    #endif

    struct permission_level_weight
    {
      permission_level permission;
      uint16_t weight;

      EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
    };

    struct key_weight
    {
      eosio::public_key key;
      uint16_t weight;

      EOSLIB_SERIALIZE(key_weight, (key)(weight))
    };

    struct wait_weight
    {
      uint32_t wait_sec;
      uint16_t weight;

      EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
    };

    struct authority
    {
      uint32_t threshold = 0;
      std::vector<key_weight> keys;
      std::vector<permission_level_weight> accounts;
      std::vector<wait_weight> waits;

      EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
    };

    struct block_header
    {
      uint32_t timestamp;
      name producer;
      uint16_t confirmed = 0;
      checksum256 previous;
      checksum256 transaction_mroot;
      checksum256 action_mroot;
      uint32_t schedule_version = 0;
      std::optional<eosio::producer_schedule> new_producers;

      EOSLIB_SERIALIZE(block_header, (timestamp)(producer)(confirmed)(previous)(transaction_mroot)(action_mroot)(schedule_version)(new_producers))
    };

    struct block_batch_info
    {
      uint32_t batch_start_height;
      time_point batch_start_timestamp;
      uint32_t batch_current_end_height;
      time_point batch_current_end_timestamp;

      EOSLIB_SERIALIZE(block_batch_info, (batch_start_height)(batch_start_timestamp)(batch_current_end_height)(batch_current_end_timestamp))
    };

    struct latest_block_batch_info_result
    {
      enum error_code_enum : uint32_t
      {
        no_error,
        invalid_input,
        unsupported_version,
        insufficient_data
      };

      std::optional<block_batch_info> result;
      error_code_enum error_code = no_error;
    };

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

// TABLES //

    struct [[eosio::table("global")]] _global_s : eosio::blockchain_parameters
    {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; };

      uint64_t max_ram_size = 512ll* 1024 * 1024 * 1024* 1024 * 1024;
      uint64_t total_ram_bytes_reserved = 0;
      int64_t total_ram_stake = 0;

      block_timestamp last_producer_schedule_update;
      uint32_t total_unpaid_blocks = 0;
      int64_t total_activated_stake = 0;

      uint16_t last_producer_schedule_size = 0;
      block_timestamp last_name_close;

      EOSLIB_SERIALIZE_DERIVED(_global_s, eosio::blockchain_parameters,
        (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
        (last_producer_schedule_update)(total_unpaid_blocks)(total_activated_stake)
        (last_producer_schedule_size)(last_name_close))
    };
    typedef singleton<name("global"), _global_s> _global;

    struct [[eosio::table("blockinfo")]] _blockinfo_s
    {
      uint8_t version = 0;
      uint32_t block_height;
      time_point block_timestamp;

      uint64_t primary_key() const { return block_height; };

      EOSLIB_SERIALIZE(_blockinfo_s, (version)(block_height)(block_timestamp))
    };
    typedef multi_index<name("blockinfo"), _blockinfo_s> _blockinfo;

    struct [[eosio::table("whitelist")]] _whitelist_s
    {
      name account;
      uint8_t depth;
      uint64_t primary_key() const { return account.value; };

      EOSLIB_SERIALIZE( _whitelist_s, (account)(depth) )
    };
    typedef multi_index<name("whitelist"), _whitelist_s> _whitelist;

    struct [[eosio::table("abihash")]] _abihash_s
    {
      name owner;
      checksum256 hash;
      uint64_t primary_key()const { return owner.value; };

      EOSLIB_SERIALIZE(_abihash_s, (owner)(hash))
    };

    struct [[eosio::table("schedules")]] _schedules_s
    {
      time_point_sec start_time;
      double continuous_rate;

      uint64_t primary_key() const { return start_time.sec_since_epoch(); };

      EOSLIB_SERIALIZE(_schedules_s, (start_time)(continuous_rate))
    };
    typedef multi_index<name("schedules"), _schedules_s> _schedules;

    struct [[eosio::table("producers")]] _producers_s
    {
      name owner;
      double total_votes = 0;
      public_key producer_key;
      bool is_active = true;
      std::string url;
      uint32_t unpaid_blocks = 0;
      time_point last_claim_time;
      uint16_t location = 0;
      eosio::binary_extension<eosio::block_signing_authority> producer_authority;

      uint64_t primary_key() const { return owner.value; };
      double by_votes() const { return is_active ? -total_votes : total_votes; };
      bool active() const { return is_active; };
      void deactivate() { producer_key = public_key(); producer_authority.reset(); is_active = false; };

      eosio::block_signing_authority get_producer_authority() const {
        if(producer_authority.has_value()){
          bool zero_threshold = std::visit( [](auto&& auth ) -> bool {
              return (auth.threshold == 0);
          }, *producer_authority );
          if( !zero_threshold ) return *producer_authority;
        }
        return eosio::block_signing_authority_v0{ .threshold = 1, .keys = {{producer_key, 1}} };
      }

      template<typename DataStream>
      friend DataStream& operator << (DataStream & ds, const _producers_s & t){
         ds << t.owner
            << t.total_votes
            << t.producer_key
            << t.is_active
            << t.url
            << t.unpaid_blocks
            << t.last_claim_time
            << t.location;

         if( !t.producer_authority.has_value() ) return ds;

         return ds << t.producer_authority;
      }

      template<typename DataStream>
      friend DataStream& operator >> (DataStream & ds, _producers_s & t){
         return ds >> t.owner
                   >> t.total_votes
                   >> t.producer_key
                   >> t.is_active
                   >> t.url
                   >> t.unpaid_blocks
                   >> t.last_claim_time
                   >> t.location
                   >> t.producer_authority;
      }
    };
    typedef multi_index<name("producers"), _producers_s,
      indexed_by<name("prototalvote"), const_mem_fun<_producers_s, double, &_producers_s::by_votes>>
    >_producers;

    struct [[eosio::table("finkeys")]] _finkeys_s
    {
      uint64_t id;
      name finalizer_name;
      std::string finalizer_key;
      std::vector<char> finalizer_key_binary; // finalizer key in binary format in Affine little endian non-montgomery g1

      uint64_t primary_key() const { return id; };
      uint64_t by_fin_name() const { return finalizer_name.value; };
      // Use binary format to hash. It is more robust and less likely to change
      // than the base64url text encoding of it.
      // There is no need to store the hash key to avoid re-computation,
      // which only happens if the table row is modified. There won't be any
      // modification of the table rows of; it may only be removed.
      checksum256 by_fin_key() const { return eosio::sha256(finalizer_key_binary.data(), finalizer_key_binary.size()); };

      bool is_active(uint64_t finalizer_active_key_id) const { return id == finalizer_active_key_id; };
    };

    typedef multi_index<
      name("finkeys"), _finkeys_s,
      indexed_by<name("byfinname"), const_mem_fun<_finkeys_s, uint64_t, &_finkeys_s::by_fin_name>>,
      indexed_by<name("byfinkey"), const_mem_fun<_finkeys_s, checksum256, &_finkeys_s::by_fin_key>>
    >_finkeys;

    struct [[eosio::table("finalizers")]] _finalizers_s
    {
      name finalizer_name;
      uint64_t active_key_id; // finalizer's active finalizer key's id in finalizer_keys_table, for fast finding key information
      std::vector<char> active_key_binary; // finalizer's active finalizer key's binary format in Affine little endian non-montgomery g1
      uint32_t finalizer_key_count = 0; // number of finalizer keys registered by this finalizer

      uint64_t primary_key() const { return finalizer_name.value; };

      EOSLIB_SERIALIZE(_finalizers_s, (finalizer_name)(active_key_id)(active_key_binary)(finalizer_key_count))
    };
    typedef multi_index<name("finalizers"), _finalizers_s> _finalizers;

    struct finalizer_auth_info {
      uint64_t key_id;
      eosio::finalizer_authority fin_authority;

      finalizer_auth_info() = default;
      explicit finalizer_auth_info(const _finalizers_s & finalizer) :
        key_id(finalizer.active_key_id),
        fin_authority(eosio::finalizer_authority{
          .description = finalizer.finalizer_name.to_string(),
          .weight = 1,
          .public_key = finalizer.active_key_binary})
        {};

      bool operator==(const finalizer_auth_info & other) const {
        return key_id == other.key_id && fin_authority.public_key == other.fin_authority.public_key;
      };

      EOSLIB_SERIALIZE(finalizer_auth_info, (key_id)(fin_authority))
    };

    struct [[eosio::table("lastpropfins")]] _lastpropfins_s
    {
      std::vector<finalizer_auth_info> last_proposed_finalizers;

      uint64_t primary_key()const { return 0; }

      EOSLIB_SERIALIZE(_lastpropfins_s, (last_proposed_finalizers))
    };

    typedef multi_index<name("lastpropfins"), _lastpropfins_s> _lastpropfins;

    // A single entry storing next available finalizer key_id to make sure
    // key_id in finalizers_table will never be reused.
    struct [[eosio::table("finkeyidgen")]] _finkeyidgen_s {
      uint64_t next_finalizer_key_id = 0;
      uint64_t primary_key()const { return 0; };

      EOSLIB_SERIALIZE(_finkeyidgen_s, (next_finalizer_key_id))
    };
    typedef multi_index<name("finkeyidgen"), _finkeyidgen_s> _finkeyidgen;

    struct [[eosio::table("limitauthchg")]] _limitauthchg_s
    {
      uint8_t version = 0;
      name account;
      std::vector<name> allow_perms;
      std::vector<name> disallow_perms;

      uint64_t primary_key() const { return account.value; };

      EOSLIB_SERIALIZE(_limitauthchg_s, (version)(account)(allow_perms)(disallow_perms))
    };
    typedef multi_index<name("limitauthchg"), _limitauthchg_s> _limitauthchg;

    struct [[eosio::table("voters")]] _voters_s
    {
      name owner;
      name proxy;
      std::vector<name> producers;
      int64_t staked = 0;
      double last_vote_weight = 0;

      double proxied_vote_weight = 0;
      bool is_proxy = 0;
      uint32_t flags1 = 0;
      uint32_t reserved2 = 0;
      eosio::asset reserved3;

      uint64_t primary_key()const { return owner.value; }

      enum class flags1_fields : uint32_t {
        ram_managed = 1,
        net_managed = 2,
        cpu_managed = 4
      };

      EOSLIB_SERIALIZE( _voters_s, (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(flags1)(reserved2)(reserved3))
    };
    typedef multi_index<name("voters"), _voters_s> _voters;

    struct [[eosio::table("userres")]] _userres_s
    {
      name owner;
      asset net_weight;
      asset cpu_weight;
      int64_t ram_bytes = user_ram_limit;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; };
      uint64_t primary_key()const { return owner.value; };

      EOSLIB_SERIALIZE(_userres_s, (owner)(net_weight)(cpu_weight)(ram_bytes))
    };
    typedef multi_index<name("userres"), _userres_s> _userres;

    struct [[eosio::table("namebids")]] _namebids_s
    {
      name newname;
      name high_bidder;
      int64_t high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
      time_point last_bid_time;

      uint64_t primary_key() const { return newname.value; };
      uint64_t by_high_bid() const { return static_cast<uint64_t>(-high_bid); };
    };
    typedef multi_index<name("namebids"), _namebids_s,
      indexed_by<name("highbid"), const_mem_fun<_namebids_s, uint64_t, &_namebids_s::by_high_bid>>
    >_namebids;

    struct [[eosio::table("bidrefunds")]] _bidrefunds_s
    {
      name bidder;
      asset amount;

      uint64_t primary_key()const { return bidder.value; };
    };
    typedef multi_index<name("bidrefunds"), _bidrefunds_s> _bidrefunds;

// 0.NATIVE ACTIONS
    void check_auth_change(name contract, name account, binary_extension<name> authorized_by);
    [[eosio::action]] void limitauthchg(const name & account, const std::vector<name> & allow_perms, const std::vector<name> & disallow_perms);

    [[eosio::action]] void updateauth(name account, name permission, name parent, authority auth, binary_extension<name> authorized_by){
      check_auth_change(get_self(), account, authorized_by);
    }
    [[eosio::action]] void deleteauth(name account, name permission, binary_extension<name> authorized_by){
      check_auth_change(get_self(), account, authorized_by);
    }
    [[eosio::action]] void linkauth(name account, name code, name type, name requirement, binary_extension<name> authorized_by){
      check_auth_change(get_self(), account, authorized_by);
    }
    [[eosio::action]] void unlinkauth(name account, name code, name type, binary_extension<name> authorized_by){
      check_auth_change(get_self(), account, authorized_by);
    }

    [[eosio::action]] void canceldelay(ignore<permission_level> canceling_auth, ignore<checksum256> trx_id){}
    [[eosio::action]] void onerror(ignore<uint128_t> sender_id, ignore<std::vector<char>> sent_trx){
      eosio::check(false, "the onerror action cannot be called directly");
    }

// 0.WHITELISTED NATIVE ACTIONS
    [[eosio::action]] void newaccount(const name & creator, const name & name, ignore<authority> owner, ignore<authority> active);
    [[eosio::action]] void setabi(const name & account, const std::vector<char> & abi, const binary_extension<std::string> & memo);
    [[eosio::action]] void setcode(const name & account, uint8_t vmtype, uint8_t vmversion, const std::vector<char> & code, const binary_extension<std::string> & memo){}

// 1.CONFIG ACTIONS
    [[eosio::action]] void init(bool destruct, const std::string & memo);
    [[eosio::action]] void setparams(const blockchain_parameters_t & params);
    [[eosio::action]] void wasmcfg(const name & settings);
    [[eosio::action]] void activate(const eosio::checksum256 & feature_digest);
    [[eosio::action]] void setram(uint64_t max_ram_size);

// 2.ADMIN ACTIONS
    [[eosio::action]] void setwhitelist(const name & account, uint8_t depth);
    [[eosio::action]] void setpriv(const name & account, uint8_t is_priv);
    [[eosio::action]] void rmvproducer(const name & producer);
    [[eosio::action]] void setacctram(const name & account);

// 3.PRODUCERS ACTIONS
    [[eosio::action]] void regproducer(const name & producer, const eosio::block_signing_authority & producer_authority, const std::string & url, const uint16_t & location);
    [[eosio::action]] void unregprod(const name & producer);

    [[eosio::action]] void regfinkey(const name & finalizer_name, const std::string & finalizer_key, const std::string & proof_of_possession);
    [[eosio::action]] void actfinkey(const name & finalizer_name, const std::string & finalizer_key);
    [[eosio::action]] void delfinkey(const name & finalizer_name, const std::string & finalizer_key);
    [[eosio::action]] void switchtosvnn();

// 4.TOKENOMICS ACTIONS
    [[eosio::action]] void logsystemfee(const name & protocol, const asset & fee, const std::string & memo);
    [[eosio::action]] void onblock(ignore<block_header> header);
    [[eosio::action]] void claimrewards(const name & owner);

// 5.USER ACTIONS
    [[eosio::action]] void voteproducer(const name & voter, const name & proxy, const std::vector<name> & producers);
    [[eosio::action]] void voteupdate(const name & voter_name);
    [[eosio::action]] void regproxy(const name & proxy, bool isproxy);

    [[eosio::action]] void bidname(const name & bidder, const name & newname, const asset & bid);
    [[eosio::action]] void bidrefund(const name & bidder, const name & newname);

  private:

// 3.FINALIZERS FUNCTIONS
    std::optional<std::vector<systemcore::finalizer_auth_info>> _last_prop_finalizers_cached;

    eosio::bls_g1 to_binary(const std::string & finalizer_key);
    eosio::checksum256 get_finalizer_key_hash(const std::string & finalizer_key);
    eosio::checksum256 get_finalizer_key_hash(const eosio::bls_g1 & finalizer_key_binary);

    bool is_savanna_consensus(_lastpropfins & lastpropfins);
    std::vector<systemcore::finalizer_auth_info> & get_last_proposed_finalizers(_lastpropfins & lastpropfins);
    void set_proposed_finalizers(std::vector<systemcore::finalizer_auth_info> finalizers, _lastpropfins & lastpropfins);
    void update_elected_producers(const block_timestamp & timestamp);

    systemcore::_finalizers::const_iterator get_finalizer_itr(const name & finalizer_name, _finalizers & finalizers) const;
    uint64_t get_next_finalizer_key_id(_finkeyidgen & finkeyidgen);

// 4.BLOCKS ACTIONS
    uint32_t block_height_from_id(const eosio::checksum256 & block_id) const;
    void add_to_blockinfo_table(const eosio::checksum256 & previous_block_id, const eosio::block_timestamp timestamp) const;

    systemcore::latest_block_batch_info_result get_latest_block_batch_info(uint32_t batch_start_height_offset, uint32_t batch_size, name system_account_name = name("eosio"));

// 5.VOTING FUNCTIONS
    int64_t update_voting_power(const name & voter, const asset & total_update);
    void update_votes(const name & voter, const name & proxy, const std::vector<name> & producers, bool voting);
};