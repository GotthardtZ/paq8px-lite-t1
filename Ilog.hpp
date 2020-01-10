#ifndef PAQ8PX_ILOG_HPP
#define PAQ8PX_ILOG_HPP

#include <xmmintrin.h>
#include "Array.hpp"
#include "cstdint"

/**
 * ilog(x) = round(log2(x) * 16), 0 <= x < 64K
 */
class Ilog {
public:
    static Ilog *getInstance();
    [[nodiscard]] int log(uint16_t x) const;
private:
    Ilog() {
      // Compute lookup table by numerical integration of 1/x
      uint32_t x = 14155776;
      for( uint32_t i = 2; i < 65536; ++i ) {
        x += 774541002 / (i * 2 - 1); // numerator is 2^29/ln 2
        t[i] = x >> 24U;
      }
    };

    Ilog(Ilog const &) {};             // copy constructor is private
    Ilog &operator=(Ilog const &) { return *this; };  // assignment operator is private
    static Ilog *mPInstance;
    Array<uint8_t> t = Array<uint8_t>(65536);
};

/**
 * llog(x) accepts 32 bits
 * @param x
 * @return
 */
int llog(uint32_t x);
uint32_t bitCount(uint32_t v);
int VLICost(uint64_t n);

/**
 * Returns floor(log2(x)).
 * 0/1->0, 2->1, 3->1, 4->2 ..., 30->4,  31->4, 32->5,  33->5
 * @param x
 * @return
 */
uint32_t ilog2(uint32_t x);

float rsqrt(float x);

#endif //PAQ8PX_ILOG_HPP
