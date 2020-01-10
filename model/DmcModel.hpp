#ifndef PAQ8PX_DMCMODEL_HPP
#define PAQ8PX_DMCMODEL_HPP

#include <cstdint>
#include <cassert>
#include "../Shared.hpp"
#include "../Random.hpp"
#include "../StateMap.hpp"
#include "../utils.hpp"
#include "../Array.hpp"
#include "../StateTable.hpp"
#include "../Stretch.hpp"
#include "../Mixer.hpp"

/**
 * c0,c1: adaptive counts of zeroes and ones;
 * fixed point numbers with 6 integer and 10 fractional bits, i.e. scaling factor = 1024;
 * thus the values 0 .. 65535 represent real counts of 0.0 .. 63.999
 * nx0, nx1: indexes of next DMC nodes in the state graph
 * state: bit history state - as in a contextmap
 */
struct DMCNode { // 12 bytes
private:
    uint32_t _nx0, _nx1; // packed: their higher 28 bits are nx0, nx1; the lower 4+4 bits give the bit history state byte

public:
    uint16_t c0, c1;

    [[nodiscard]] uint8_t getState() const { return uint8_t(((_nx0 & 0xFU) << 4U) | (_nx1 & 0xFU)); }

    void setState(const uint8_t state) {
      _nx0 = (_nx0 & 0xfffffff0U) | (state >> 4U);
      _nx1 = (_nx1 & 0xfffffff0U) | (state & 0xFU);
    }

    [[nodiscard]] uint32_t getNx0() const { return _nx0 >> 4U; }

    void setNx0(const uint32_t nx0) {
      assert((nx0 >> 28) == 0);
      _nx0 = (_nx0 & 0xFU) | (nx0 << 4U);
    }

    [[nodiscard]] uint32_t getNx1() const { return _nx1 >> 4U; }

    void setNx1(const uint32_t nx1) {
      assert((nx1 >> 28) == 0);
      _nx1 = (_nx1 & 0xFU) | (nx1 << 4U);
    }
};

#define DMC_NODES_BASE (255 * 256) // = 65280
#define DMC_NODES_MAX ((uint64_t(1) << 31) / sizeof(DMCNode)) // = 178 956 970

/**
 * Model using DMC (Dynamic Markov Compression).
 *
 * The bitwise context is represented by a state graph.
 *
 * See the original paper: http://webhome.cs.uvic.ca/~nigelh/Publications/DMC.pdf
 * See the original DMC implementation: http://maveric0.uwaterloo.ca/ftp/dmc/
 *
 * Main differences:
 * - Instead of floats we use fixed point arithmetic.
 * - The threshold for cloning a state increases gradually as memory is used up.
 * - For probability estimation each state maintains both a 0,1 count ("c0" and "c1")
 *   and a bit history ("state"). The 0,1 counts are updated adaptively favoring newer events.
 *   The bit history state is mapped to a probability adaptively using a StateMap.
 * - The predictions of multiple "DmcModel"s are combined and stabilized in "dmcForest". See below.
 */
class DmcModel {
private:
    Shared *shared = Shared::getInstance();
    Random rnd;
    Array<DMCNode> t; // state graph
    StateMap sm; // stateMap for bit history states
    uint32_t top, curr; // index of first unallocated node (i.e. number of allocated nodes); index of current node
    uint32_t threshold; // cloning threshold parameter: fixed point number like c0,c1
    uint32_t thresholdFine; // "threshold" scaled by 11 bits used for increasing the threshold in finer steps
    uint32_t extra; // this value is used for approximating state graph maturity level when the state graph is already full
    // this is the number of skipped cloning events when the counts were already large enough (>1.0)

    // helper function: adaptively increment a counter
    [[nodiscard]] uint32_t
    incrementCounter(const uint32_t x, const uint32_t increment) const { // x is a fixed point number as c0,c1 ; "increment"  is 0 or 1
      return (((x << 6U) - x) >> 6U) + (increment << 10U); // x * (1-1/64) + increment
    }

public:
    DmcModel(const uint64_t dmcNodes, const uint32_t thStart) : t(min(dmcNodes + DMC_NODES_BASE, DMC_NODES_MAX)),
            sm(1, 256, 256 /*64-512 are all fine*/, StateMap::BitHistory) //StateMap: s, n, limit, init
    {
      resetStateGraph(thStart);
    }

    /**
     * Initialize the state graph to a bytewise order 1 model
     * See an explanation of the initial structure in:
     * http://wing.comp.nus.edu.sg/~junping/docs/njp-icita2005.pdf
     * @param thStart
     */
    void resetStateGraph(const uint32_t thStart) {
      assert(((t.size() - 1) >> 28) ==
             0); // the top 4 bits must be unused by nx0 and nx1 for storing the 4+4 bits of the bit history state byte
      top = curr = extra = 0;
      threshold = thStart;
      thresholdFine = thStart << 11U;
      for( int j = 0; j < 256; ++j ) { //256 trees
        for( int i = 0; i < 255; ++i ) { //255 nodes in each tree
          if( i < 127 ) { //internal tree nodes
            t[top].setNx0(top + i + 1); // left node
            t[top].setNx1(top + i + 2); // right node
          } else { // 128 leaf nodes - they each references a root node of tree(i)
            int linkedTreeRoot = (i - 127) * 2 * 255;
            t[top].setNx0(linkedTreeRoot); // left node  -> root of tree 0,2,4...
            t[top].setNx1(linkedTreeRoot + 255); // right node -> root of tree 1,3,5...
          }
          t[top].c0 = t[top].c1 = thStart < 1024 ? 2048 : 512; // 2.0  0.5
          t[top].setState(0);
          top++;
        }
      }
    }

    /**
     * update state graph
     */
    void update() {
      uint32_t c0 = t[curr].c0;
      uint32_t c1 = t[curr].c1;
      INJECT_SHARED_y
      const uint32_t n = y == 0 ? c0 : c1;

      // update counts, state
      t[curr].c0 = incrementCounter(c0, 1 - y);
      t[curr].c1 = incrementCounter(c1, y);
      t[curr].setState(StateTable::next(t[curr].getState(), y, rnd));

      // clone next state when threshold is reached
      if( n > threshold ) {
        const uint32_t next = y == 0 ? t[curr].getNx0() : t[curr].getNx1();
        c0 = t[next].c0;
        c1 = t[next].c1;
        const uint32_t nn = c0 + c1;

        if( nn > n + threshold ) {
          if( top != t.size()) { // state graph is not yet full, let's clone
            uint32_t c0Top = uint64_t(c0) * uint64_t(n) / uint64_t(nn);
            uint32_t c1Top = uint64_t(c1) * uint64_t(n) / uint64_t(nn);
            assert(c0 >= c0Top);
            assert(c1 >= c1Top);
            c0 -= c0Top;
            c1 -= c1Top;

            t[top].c0 = c0Top;
            t[top].c1 = c1Top;
            t[next].c0 = c0;
            t[next].c1 = c1;

            t[top].setNx0(t[next].getNx0());
            t[top].setNx1(t[next].getNx1());
            t[top].setState(t[next].getState());
            if( y == 0 )
              t[curr].setNx0(top);
            else
              t[curr].setNx1(top);

            ++top;

            if( threshold < 8 * 1024 )
              threshold = (++thresholdFine) >> 11U;
          } else // state graph was full
            extra += nn >> 10U;
        }
      }

      if( y == 0 )
        curr = t[curr].getNx0();
      else
        curr = t[curr].getNx1();
    }

    [[nodiscard]] bool isFull() const { return extra >> 7U > uint32_t(t.size()); }

    [[nodiscard]] int pr1() const {
      const uint32_t n0 = t[curr].c0 + 1;
      const uint32_t n1 = t[curr].c1 + 1;
      return (n1 << 12U) / (n0 + n1);
    }

    int pr2() {
      const uint8_t state = t[curr].getState();
      return sm.p1(state);
    }

    int st() {
      update();
      return stretch(pr1()) + stretch(pr2()); // average the predictions for stability
    }
};

#include "DmcForest.hpp"

#endif //PAQ8PX_DMCMODEL_HPP