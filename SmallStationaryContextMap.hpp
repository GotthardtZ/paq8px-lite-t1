#ifndef PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP
#define PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP

#include "IPredictor.hpp"
#include "UpdateBroadcaster.hpp"
#include "Mixer.hpp"
#include "Stretch.hpp"

/**
 * map for modelling contexts of (nearly-)stationary data.
 * The context is looked up directly. For each bit modelled, a 16bit prediction is stored.
 * The adaptation rate is controlled by the caller, see mix().
 *
 * - BitsOfContext: How many bits to use for each context. Higher bits are discarded.
 * - InputBits: How many bits [1..8] of input are to be modelled for each context.
 * New contexts must be set at those intervals.
 *
 * Uses (2^(BitsOfContext+1))*((2^InputBits)-1) bytes of memory.
 */
class SmallStationaryContextMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 2;

private:
    Shared *shared = Shared::getInstance();
    Array<uint16_t> data;
    const uint32_t mask;
    const uint32_t stride;
    const uint32_t bTotal;
    uint32_t b {};
    uint32_t context {};
    uint32_t bCount {};
    uint16_t *cp {};
    const int rate;
    int scale;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    SmallStationaryContextMap(int bitsOfContext, int inputBits, int rate, int scale);
    void set(uint32_t ctx);
    void reset();
    void update() override;
    void setScale(int Scale);
    void mix(Mixer &m);
};

#endif //PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP
