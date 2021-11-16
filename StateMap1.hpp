#ifndef PAQ8PX_STATEMAP1_HPP
#define PAQ8PX_STATEMAP1_HPP

#include <cstdint>

/**
 * A @ref StateMap maps a context to a probability.
 */
class StateMap1 {
public:

    auto p1(uint8_t state) -> int;

};

#endif //PAQ8PX1_STATEMAP_HPP
