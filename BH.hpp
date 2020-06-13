#ifndef PAQ8PX_BH_HPP
#define PAQ8PX_BH_HPP

#include "Array.hpp"
#include "Hash.hpp"
#include "utils.hpp"
#include <cstdint>

/**
 * A BH maps a 32 bit hash to an array of b bytes (checksum and b-2 values)
 * The index must be a multiplicative hash.
 *
 * If a collision is detected, up to m nearby locations in the same
 * cache line are tested and the first matching checksum or
 * empty element is returned.
 *
 * Collision detection and resolution policy:
 * 2 byte checksum with LRU replacement
 * If neither a match nor an empty element is found the lowest priority
 * element is replaced by the new element (except last 2 by priority).

 * @tparam B
 */
template<uint64_t B>
class BH {
    uint32_t searchLimit = 8;
    Array<uint8_t> t; /**< elements */
    const uint32_t mask; /**< size-1 */
    const int hashBits;

public:
    /**
     * Creates @ref n element table with @ref b bytes each. @ref n must be a power of 2.
     * The first byte of each element is reserved for a checksum to detect collisions.
     * The remaining b-1 bytes are values, prioritized by the first value. This byte is 0 to mark an unused element.
     * @param n number of elements in the table
     */
    explicit BH(uint64_t n) : t(n * B), mask(uint32_t(n - 1)), hashBits(ilog2(mask + 1)) {
      assert(B >= 2 && n > 0 && isPowerOf2(n));
    }

    /**
     * Returns a pointer to the ctx'th element, such that
     * bh[ctx][0] is a checksum of @ref ctx, bh[ctx][1] is the priority, and
     * bh[ctx][2..b-1] are other values (0-255).
     * @tparam B number of bytes
     * @param ctx index
     * @return pointer to the ctx'th element
     */
    auto operator[](uint64_t ctx) -> uint8_t *;
};

template<uint64_t B>
inline auto BH<B>::operator[](const uint64_t ctx) -> uint8_t * {
  const uint16_t chk = checksum16(ctx, hashBits);
  const uint32_t i = finalize64(ctx, hashBits) * searchLimit & mask;
  uint8_t *p = nullptr;
  uint16_t *cp = nullptr;
  int j = 0;
  for( j = 0; j < searchLimit; ++j ) {
    p = &t[(i + j) * B];
    cp = reinterpret_cast<uint16_t *>(p);
    if( p[2] == 0 ) {
      *cp = chk;
      break;
    } // empty slot
    if( *cp == chk ) {
      break; // found
    }
  }
  if( j == 0 ) {
    return p + 1; // front
  }
  static uint8_t tmp[B]; // element to move to front
  if( j == searchLimit ) {
    --j;
    memset(tmp, 0, B);
    memmove(tmp, &chk, 2);
    if( searchLimit > 2 && t[(i + j) * B + 2] > t[(i + j - 1) * B + 2] ) {
      --j;
    }
  } else {
    memcpy(tmp, cp, B);
  }
  memmove(&t[(i + 1) * B], &t[i * B], j * B);
  memcpy(&t[i * B], tmp, B);
  return &t[i * B + 1];
}

#endif //PAQ8PX_BH_HPP
