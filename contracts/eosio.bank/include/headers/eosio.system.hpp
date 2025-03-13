#pragma once

#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

using namespace eosio;
namespace systemcore
{
  static constexpr uint32_t lucky_number_odds = 16 * 16; // * 16; // Out of == 4096;
  static constexpr uint32_t lucky_number_chunks = 6 * 30 * 24 * 3600 / lucky_number_odds; // Approx 6 months || 180 days == 3796.875;

  struct _aluckynumber_s
  {
    eosio::checksum256 seed;
    uint32_t odds = lucky_number_odds;

    uint32_t epoch = 1;
    uint32_t chunks_remaining = lucky_number_chunks;

    uint32_t blocks_since = 0;
    uint64_t total_blocks = 0;

    name producer;
  };
  typedef singleton<name("aluckynumber"), _aluckynumber_s> _aluckynumber;
};
