#ifndef PAQ8PX_CLZ_HPP
#define PAQ8PX_CLZ_HPP

#include <cstdint>

static inline auto clz(uint32_t value) -> uint32_t {
  return __builtin_clz(value);
}

static inline auto ctz(uint32_t value) -> uint32_t {
  return __builtin_ctz(value);
}

#endif PAQ8PX_CLZ_HPP