#pragma once

#include <array>

class RandomnessProvider
{
  public:
    RandomnessProvider(eosio::checksum256 seed, uint64_t pepper = u32_max)
    {
      seed_bytes = seed.extract_as_byte_array();
      salt = std::max(pepper, u32_max);
      cycle = 1;
    }

    void regen(eosio::checksum256 seed, uint64_t pepper = u32_max)
    {
      seed_bytes = seed.extract_as_byte_array();
      salt = std::max(pepper, u32_max);
      cycle = 1;
    }

    eosio::checksum256 get_seed(uint64_t pepper = u32_max)
    {
      pepper = pepper > 1000 ? pepper : u32_max;
      seed_bytes[pepper % 32] = pepper % 256;
      uint64_t mixer = get_rand((uint32_t)pepper);
      seed_bytes[mixer % 32] = mixer % 256;

      eosio::checksum256 new_seed = eosio::sha256((char *)seed_bytes.data(), 32);
      seed_bytes = new_seed.extract_as_byte_array();

      return new_seed;
    }

    eosio::checksum256 get_blend(eosio::checksum256 seed, uint64_t pepper = u32_max)
    {
      pepper = pepper > 1000 ? pepper : u32_max;
      std::array<uint8_t, 32> raw32 = seed.extract_as_byte_array();
      std::array<uint8_t, 64> raw64;
      for (uint8_t i = 0; i <32; i++){
        raw64[i * 2] = raw32[i];
        raw64[i * 2 + 1] = seed_bytes[i];
      }

      eosio::checksum512 pass512 = eosio::sha512((char *)raw64.data(), 64);
      raw64 = pass512.extract_as_byte_array();

      for (uint8_t i = 0; i <32; i++){
        raw32[i] = raw64[i * 1.5 + 4];
        seed_bytes[i] = raw64[i * 2];
      }
      eosio::checksum256 new_hash = eosio::sha256((char *)raw32.data(), 32);
      return new_hash;
    }


    uint64_t get_uint64()
    {
      uint64_t value = u32_max;
      uint64_t shuffle = 0;

      if (cycle > 283){
          cycle = salt % 88 + 1;
      }

      for (auto a = 0; a < 13; a++){
        salt = (((salt << 12) | (seed_bytes[shuffle] << 8)) | seed_bytes[salt % 32]) & u64_max;
        seed_bytes[(salt & 65535) % 32] = salt % 255;

        shuffle = salt % (9001 * cycle);

        while (shuffle >= 32){
            shuffle = (shuffle >> 1) & 255;
        }

        value = (((value << 16) | (seed_bytes[salt % 32] << 8)) | seed_bytes[shuffle]) & u64_max;
        ++cycle;
      }

      return value;
    }

    uint32_t get_rand(uint32_t max_value)
    {
      uint64_t roll = get_uint64();
      if (max_value >= 65535){
          return roll % max_value;
      } else {
          return (uint32_t)((double)(roll % 1000000) * ((double)max_value / 1000000.0));
      }
    }




  private:

  std::array<uint8_t, 32> seed_bytes;
  uint64_t salt;
  uint64_t cycle;

  static constexpr uint64_t u32_max = 4294967295;
  static constexpr uint64_t u64_max = 18446744073709551615U;
};