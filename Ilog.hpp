#ifndef PAQ8PX_ILOG_HPP
#define PAQ8PX_ILOG_HPP

/**
 * ilog(x) = round(log2(x) * 16), 0 <= x < 64K
 */
class Ilog {
    Array<uint8_t> t;

public:
    int operator()(uint16_t x) const { return (int) t[x]; }

    Ilog() : t(65536) {
      // Compute lookup table by numerical integration of 1/x
      uint32_t x = 14155776;
      for( uint32_t i = 2; i < 65536; ++i ) {
        x += 774541002 / (i * 2 - 1); // numerator is 2^29/ln 2
        t[i] = x >> 24U;
      }
    }
} ilog;

/**
 * llog(x) accepts 32 bits
 * @param x
 * @return
 */
inline int llog(uint32_t x) {
  if( x >= 0x1000000 )
    return 256 + ilog(uint16_t(x >> 16U));
  else if( x >= 0x10000 )
    return 128 + ilog(uint16_t(x >> 8U));
  else
    return ilog(uint16_t(x));
}

inline uint32_t bitCount(uint32_t v) {
  v -= ((v >> 1U) & 0x55555555U);
  v = ((v >> 2U) & 0x33333333U) + (v & 0x33333333U);
  v = ((v >> 4U) + v) & 0x0f0f0f0fU;
  v = ((v >> 8U) + v) & 0x00ff00ffU;
  v = ((v >> 16U) + v) & 0x0000ffffU;
  return v;
}

inline int VLICost(uint64_t n) {
  int cost = 1;
  while( n > 0x7F ) {
    n >>= 7U;
    cost++;
  }
  return cost;
}

// ilog2
// returns floor(log2(x))
// 0/1->0, 2->1, 3->1, 4->2 ..., 30->4,  31->4, 32->5,  33->5
#ifdef _MSC_VER
#include <intrin.h>
inline uint32_t ilog2(uint32_t x) {
  DWORD tmp = 0;
  if (x != 0) _BitScanReverse(&tmp, x);
  return tmp;
}
#elif __GNUC__

inline uint32_t ilog2(uint32_t x) {
  if( x != 0 )
    x = 31 - __builtin_clz(x);
  return x;
}

#else
inline uint32_t ilog2(uint32_t x) {
  //copy the leading "1" bit to its left (0x03000000 -> 0x03ffffff)
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  //how many trailing bits do we have (except the first)?
  return BitCount(x >> 1);
}
#endif

inline float rsqrt(const float x) {
  float r = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); //SSE
  return (0.5f * (r + 1.0f / (x * r)));
}

#endif //PAQ8PX_ILOG_HPP
