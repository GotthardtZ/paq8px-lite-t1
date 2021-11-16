#ifndef PAQ8PX_STATEMAP_HPP
#define PAQ8PX_STATEMAP_HPP

#include "DivisionTable.hpp"
#include "Shared.hpp"
#include "StateTable.hpp"

/**
 * A @ref StateMap maps a context to a probability.
 */
class StateMap {
private:
  const Shared* const shared;
  const uint32_t numContextsPerSet; /**< Number of contexts in each context set */
  Array<uint32_t> t; /**< cxt -> prediction in high 22 bits, count in low 10 bits */
  int limit;
  uint32_t cxt; /**< context index of last prediction per context set */
  int* dt; /**< Pointer to division table */

public:
    enum MAPTYPE {
        Generic, BitHistory, Run
    };

    /**
     * Creates a @ref StateMap with @ref n contexts using 4*n bytes memory.
     * @param s
     * @param n number of contexts
     * @param lim
     * @param mapType
     */
    StateMap(const Shared* const sh, int n, int lim, MAPTYPE mapType);

    void update();

    /**
     * @param cx
     * @return
     */
    auto p1(uint32_t cx) -> int;

    void print() const;
};

#endif //PAQ8PX_STATEMAP_HPP
