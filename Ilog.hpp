#ifndef PAQ8PX_ILOG_HPP
#define PAQ8PX_ILOG_HPP

#include "Array.hpp"
#include "cstdint"
#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#elif defined(__ARM_FEATURE_SIMD32) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/**
 * ilog(x) = round(log2(x) * 16), 0 <= x < 64K
 */
class Ilog {
public:
    static auto getInstance() -> Ilog *;
    [[nodiscard]] auto log(uint16_t x) const -> int;
private:
    /**
     * Compute lookup table by numerical integration of 1/x
     */
    Ilog() {
      uint32_t x = 14155776;
      for( uint32_t i = 2; i < 65536; ++i ) {
        x += 774541002 / (i * 2 - 1); // numerator is 2^29/ln 2
        t[i] = x >> 24U;
      }
    }

    /**
     * Copy constructor is private so that it cannot be called
     */
    Ilog(Ilog const & /*unused*/) {}

    /**
     * Assignment operator is private so that it cannot be called
     */
    auto operator=(Ilog const & /*unused*/) -> Ilog & { return *this; }

    static Ilog *mPInstance;
    Array<uint8_t> t = Array<uint8_t>(65536);
};

/**
 * llog(x) accepts 32 bits
 * @param x
 * @return
 */
auto llog(uint32_t x) -> int;
auto bitCount(uint32_t v) -> uint32_t;
auto VLICost(uint64_t n) -> int;

auto rsqrt(float x) -> float;

#endif //PAQ8PX_ILOG_HPP
