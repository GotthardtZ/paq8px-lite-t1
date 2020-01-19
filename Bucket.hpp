#ifndef PAQ8PX_BUCKET_HPP
#define PAQ8PX_BUCKET_HPP

#include <cstdint>
#include <emmintrin.h>

static uint32_t inline clz(uint32_t value) {
  return __builtin_clz(value);
}

static uint32_t inline ctz(uint32_t value) {
  return __builtin_ctz(value);
}

/**
 * Hash bucket, 64 bytes
 */
class Bucket {
    uint16_t checksums[7]; /**< byte context checksums */
    uint8_t mostRecentlyUsed; /**< last 2 accesses (0-6) in low, high nibble */
public:
    uint8_t bitState[7][7]; /**< byte context, 3-bit context -> bit history state */
    // bitState[][0] = 1st bit, bitState[][1,2] = 2nd bit, bitState[][3..6] = 3rd bit
    // bitState[][0] is also a replacement priority, 0 = empty

    /**
     * Find or create hash element matching checksum.
     * If not found, insert or replace lowest priority (skipping 2 most recent).
     * @param checksum
     * @return
     */
    inline uint8_t *find(const uint16_t checksum) {
//      if( checksums[mostRecentlyUsed & 15U] == checksum )
//        return &bitState[mostRecentlyUsed & 15U][0];
//      int worst = 0xFFFF, idx = 0;
//      for( int i = 0; i < 7; ++i ) {
//        if( checksums[i] == checksum ) {
//          mostRecentlyUsed = mostRecentlyUsed << 4U | i;
//          return &bitState[i][0];
//        }
//        if( bitState[i][0] < worst && (mostRecentlyUsed & 15U) != i && mostRecentlyUsed >> 4U != i ) {
//          worst = bitState[i][0];
//          idx = i;
//        }
//      }
//      mostRecentlyUsed = 0xF0U | idx;
//      checksums[idx] = checksum;
//      return (uint8_t *) memset(&bitState[idx][0], 0, 7);
      if( checksums[mostRecentlyUsed & 15U] == checksum )
        return &bitState[mostRecentlyUsed & 15U][0];
      int worst = 0xFFFF;
      int idx = 0;
      const __m128i xmmChecksum = _mm_set1_epi16(short(checksum)); //fill 8 ch values
      // load 8 values, discard last one as only 7 are needed.
      // reverse order and compare 7 checksums values to @ref checksum
      // get mask is set get first index and return value
      __m128i tmp = _mm_load_si128((__m128i *) &checksums[0]); //load 8 values (8th will be discarded)

      tmp = _mm_shuffle_epi8(tmp, _mm_setr_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1));
      tmp = _mm_cmpeq_epi16(tmp, xmmChecksum); // compare ch values
      tmp = _mm_packs_epi16(tmp, _mm_setzero_si128()); // pack result
      uint32_t t = (_mm_movemask_epi8(tmp)) >> 1; // get mask of comparison, bit is set if eq, discard 8th bit
      uint32_t a;    // index into bitState or 7 if not found
      if( t ) {
        a = (clz(t) - 1) & 7U;
        mostRecentlyUsed = mostRecentlyUsed << 4U | a;
        return (uint8_t *) &bitState[a][0];
      }

      __m128i lastL = _mm_set1_epi8((mostRecentlyUsed & 15U));
      __m128i lastH = _mm_set1_epi8((mostRecentlyUsed >> 4U));
      __m128i one1 = _mm_set1_epi8(1);
      __m128i vm = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7);

      __m128i lastX = _mm_unpacklo_epi64(lastL, lastH); // mostRecentlyUsed&15 mostRecentlyUsed>>4
      __m128i eq0 = _mm_cmpeq_epi8(lastX, vm); // compare values

      eq0 = _mm_or_si128(eq0, _mm_srli_si128 (eq0, 8));    //or low values with high

      lastX = _mm_and_si128(one1, eq0);                //set to 1 if eq
      __m128i sum1 = _mm_sad_epu8(lastX, _mm_setzero_si128());        // count values, abs(a0 - b0) + abs(a1 - b1) .... up to b8
      const uint32_t pCount = _mm_cvtsi128_si32(sum1); // population count
      uint32_t t0 = (~_mm_movemask_epi8(eq0));
      for( int i = pCount; i < 7; ++i ) {
        int bitt = ctz(t0);     // get index
        t0 &= ~(1 << bitt); // clear bit set and test again
        int pri = bitState[bitt][0];
        if( pri < worst ) {
          worst = pri;
          idx = bitt;
        }
      }
      mostRecentlyUsed = 0xF0U | idx;
      checksums[idx] = checksum;
      return (uint8_t *) memset(&bitState[idx][0], 0, 7);
    }
};

#endif //PAQ8PX_BUCKET_HPP
