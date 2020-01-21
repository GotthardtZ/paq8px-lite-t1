#ifndef PAQ8PX_RANDOM_HPP
#define PAQ8PX_RANDOM_HPP

#include "Array.hpp"
#include <cstdint>

/**
 * 32-bit pseudo random number generator
 */
class Random {
    Array<uint32_t> table;
    uint64_t i;

public:
    Random();
    auto operator()() -> uint32_t;
};

#endif //PAQ8PX_RANDOM_HPP
