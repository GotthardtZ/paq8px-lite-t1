#ifndef PAQ8PX_RUNMAP1_HPP
#define PAQ8PX_RUNMAP1_HPP

#include <cstdint>

/**
 * A @ref RunMap maps a context to a probability.
 */
class RunMap1 {
public:

    auto p1(uint32_t runstate) -> int;

};

#endif //PAQ8PX_RUNMAP1_HPP
