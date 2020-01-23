#ifndef PAQ8PX_HASH_HPP
#define PAQ8PX_HASH_HPP

#include "utils.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>

//////////////////////// Hash functions //////////////////////////
//
// All hash functions are constructed using multiplicative hashes
// - We usually hash small values
// - Multiplicative hashes promote entropy to the higher bits
// - When combining ( H(x) + H(y) ) entropy is still in higher bits
// - After combining they must be finalized by taking the higher
//   bits only to reduce the range to the desired hash table size

// Multipliers
// - They don't need to be prime, just large odd numbers
// - The golden ratio is usually preferred as a multiplier (PHI64)

#ifdef NHASHCONFIG
#define HASHEXPR constexpr
#else
#define HASHEXPR
#endif

static HASHEXPR uint64_t hashes[14] = {UINT64_C(0x9E3779B97F4A7C15), UINT64_C(0x993DDEFFB1462949), UINT64_C(0xE9C91DC159AB0D2D),
                                       UINT64_C(0x83D6A14F1B0CED73), UINT64_C(0xA14F1B0CED5A841F), UINT64_C(0xC0E51314A614F4EF),
                                       UINT64_C(0xDA9CC2600AE45A27), UINT64_C(0x826797AA04A65737), UINT64_C(0x2375BE54C41A08ED),
                                       UINT64_C(0xD39104E950564B37), UINT64_C(0x3091697D5E685623), UINT64_C(0x20EB84EE04A3C7E1),
                                       UINT64_C(0xF501F1D0944B2383), UINT64_C(0xE3E4E8AA829AB9B5)};

#ifndef NHASHCONFIG

static void loadHashesFromCmd(const char *hashesFromCommandline) {
  if( strlen(hashesFromCommandline) != 16 * 14 + 13 /*237*/) {
    quit("Bad hash config.");
  }
  for( int i = 0; i < 14; i++ ) { // for each specified hash value
    uint64_t hashVal = 0;
    for( int j = 0; j < 16; j++ ) { // for each hex char
      uint8_t c = hashesFromCommandline[i * 17 + j];
      if( c >= 'a' && c <= 'f' ) {
        c = c + 'A' - 'a';
      }
      if( c >= '0' && c <= '9' ) {
        c -= '0';
      } else if( c >= 'A' && c <= 'F' ) {
        c = c - 'A' + 10;
      } else {
        quit("Bad hash config.");
      }
      hashVal = hashVal << 4U | c;
    }
    hashes[i] = hashVal;
  }
}

#endif //NHASHCONFIG

// Golden ratio of 2^64 (not a prime)
#define PHI64 hashes[0] // 11400714819323198485

// Some more arbitrary magic (prime) numbers
#define MUL64_1 hashes[1]
#define MUL64_2 hashes[2]
#define MUL64_3 hashes[3]
#define MUL64_4 hashes[4]
#define MUL64_5 hashes[5]
#define MUL64_6 hashes[6]
#define MUL64_7 hashes[7]
#define MUL64_8 hashes[8]
#define MUL64_9 hashes[9]
#define MUL64_10 hashes[10]
#define MUL64_11 hashes[11]
#define MUL64_12 hashes[12]
#define MUL64_13 hashes[13]

// Finalizers (range reduction)
// - Keep the necessary number of bits after performing a
//   (combination of) multiplicative hash(es)

static ALWAYS_INLINE
auto finalize64(const uint64_t hash, const int hashBits) -> uint32_t {
  assert(uint32_t(hashBits) <= 32); // just a reasonable upper limit
  return uint32_t(hash >> (64 - hashBits));
}

// Get the next 8 or 16 bits following "hashBits" for checksum
static ALWAYS_INLINE
uint64_t checksum64(const uint64_t hash, const int hashBits, const int checksumBits) {
  assert(0 < checksumBits && uint32_t(checksumBits) <= 32); //32 is just a reasonable upper limit
  return (hash >> (64 - hashBits - checksumBits)) & ((1 << checksumBits) - 1);
}

//
// value hashing
//
// - Hash 1-13 64-bit (usually small) integers
//


//template<int size, int idx = size, class dummy = void>
//struct MM{
//    static constexpr unsigned int hash(const char * str, unsigned int prev_crc = 0xFFFFFFFF)
//    {
//      return MM<size, idx+1>::crc32(str, (prev_crc >> 8) ^ crc_table[(prev_crc ^ str[idx]) & 0xFF] );
//    }
//};
//
//// This is the stop-recursion function
//template<int size, class dummy>
//struct MM<size, 0, dummy>{
//    static constexpr unsigned int hash(const uint64_t x0)
//    {
//      return (x0 + 1) * PHI64;
//    }
//};

static ALWAYS_INLINE
auto hash(const uint64_t x0) -> uint64_t {
  return (x0 + 1) * PHI64;
}

static ALWAYS_INLINE
uint64_t hash(const uint64_t x0, const uint64_t x1) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1;
}

static ALWAYS_INLINE
uint64_t hash(const uint64_t x0, const uint64_t x1, const uint64_t x2) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2;
}

static ALWAYS_INLINE
uint64_t hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3;
}

static ALWAYS_INLINE
auto hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4) -> uint64_t {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4;
}

static ALWAYS_INLINE
auto hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5) -> uint64_t {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5;
}

static ALWAYS_INLINE
uint64_t
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6;
}

static ALWAYS_INLINE
uint64_t
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7;
}

static ALWAYS_INLINE
uint64_t
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7, const uint64_t x8) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7 + (x8 + 1) * MUL64_8;
}

static ALWAYS_INLINE
auto
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7, const uint64_t x8, const uint64_t x9) -> uint64_t {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7 + (x8 + 1) * MUL64_8 + (x9 + 1) * MUL64_9;
}

static ALWAYS_INLINE
uint64_t
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7, const uint64_t x8, const uint64_t x9, const uint64_t x10) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7 + (x8 + 1) * MUL64_8 + (x9 + 1) * MUL64_9 + (x10 + 1) * MUL64_10;
}

static ALWAYS_INLINE
uint64_t
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7, const uint64_t x8, const uint64_t x9, const uint64_t x10, const uint64_t x11) {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7 + (x8 + 1) * MUL64_8 + (x9 + 1) * MUL64_9 + (x10 + 1) * MUL64_10 + (x11 + 1) * MUL64_11;
}

static ALWAYS_INLINE
auto
hash(const uint64_t x0, const uint64_t x1, const uint64_t x2, const uint64_t x3, const uint64_t x4, const uint64_t x5, const uint64_t x6,
     const uint64_t x7, const uint64_t x8, const uint64_t x9, const uint64_t x10, const uint64_t x11, const uint64_t x12) -> uint64_t {
  return (x0 + 1) * PHI64 + (x1 + 1) * MUL64_1 + (x2 + 1) * MUL64_2 + (x3 + 1) * MUL64_3 + (x4 + 1) * MUL64_4 + (x5 + 1) * MUL64_5 +
         (x6 + 1) * MUL64_6 + (x7 + 1) * MUL64_7 + (x8 + 1) * MUL64_8 + (x9 + 1) * MUL64_9 + (x10 + 1) * MUL64_10 + (x11 + 1) * MUL64_11 +
         (x12 + 1) * MUL64_12;
}

//
// String hashing
//

// Call this function repeatedly for string hashing, or to combine
// a hash value and a (non-hash) value, or two hash values
static ALWAYS_INLINE
uint64_t combine64(const uint64_t seed, const uint64_t x) {
  return hash(seed + x);
}

#endif //PAQ8PX_HASH_HPP
