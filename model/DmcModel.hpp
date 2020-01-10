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
#include "DmcNode.hpp"

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
    [[nodiscard]] uint32_t incrementCounter(uint32_t x, uint32_t increment) const;
public:
    DmcModel(uint64_t dmcNodes, uint32_t thStart);

    /**
     * Initialize the state graph to a bytewise order 1 model
     * See an explanation of the initial structure in:
     * http://wing.comp.nus.edu.sg/~junping/docs/njp-icita2005.pdf
     * @param thStart
     */
    void resetStateGraph(uint32_t thStart);

    /**
     * update state graph
     */
    void update();
    [[nodiscard]] bool isFull() const;
    [[nodiscard]] int pr1() const;
    int pr2();
    int st();
};

#endif //PAQ8PX_DMCMODEL_HPP