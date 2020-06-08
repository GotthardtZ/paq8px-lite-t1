#ifndef PAQ8PX_VLI
#define PAQ8PX_VLI

#include <cstdint>

static auto VLICost(uint64_t n) -> int {
  int cost = 1;
  while (n > 0x7F) {
    n >>= 7U;
    cost++;
  }
  return cost;
}

#endif