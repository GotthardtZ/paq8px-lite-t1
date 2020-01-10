#ifndef PAQ8PX_RANDOM_HPP
#define PAQ8PX_RANDOM_HPP

#include <cstdint>
#include "Array.hpp"

/**
 * 32-bit pseudo random number generator
 */
class Random {
    Array<uint32_t> table;
    uint64_t i;

public:
    Random();
    uint32_t operator()();
};

#endif //PAQ8PX_RANDOM_HPP
