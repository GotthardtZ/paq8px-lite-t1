#ifndef PAQ8PX_STATIONARYMAP_HPP
#define PAQ8PX_STATIONARYMAP_HPP

#include "IPredictor.hpp"
#include "UpdateBroadcaster.hpp"
#include "Mixer.hpp"
#include "DivisionTable.hpp"
#include "Stretch.hpp"
#include "Hash.hpp"

/**
 * map for modelling contexts of (nearly-)stationary data.
 * The context is looked up directly. For each bit modelled, a 32bit element stores
 * a 22 bit prediction and a 10 bit adaptation rate offset.
 *
 * - BitsOfContext: How many bits to use for each context. Higher bits are discarded.
 * - InputBits: How many bits [1..8] of input are to be modelled for each context.
 * New contexts must be set at those intervals.
 * - Rate: Initial adaptation rate offset [0..1023]. Lower offsets mean faster adaptation.
 * Will be increased on every occurrence until the higher bound is reached.
 *
 * Uses (2^(BitsOfContext+2))*((2^InputBits)-1) bytes of memory.
 */
class StationaryMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 2;

private:
    Shared *shared = Shared::getInstance();
    Array<uint32_t> data;
    const uint32_t mask, maskBits, stride, bTotal;
    uint32_t context, bCount, b;
    uint32_t *cp;
    int scale;
    const uint16_t limit;
    int *dt;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    StationaryMap(int bitsOfContext, int inputBits, int scale, uint16_t limit);

    /**
     *  ctx must be a direct context (no hash)
     * @param ctx
     */
    void setDirect(uint32_t ctx);

    /**
     * // ctx must be a hash
     * @param ctx
     */
    void set(uint64_t ctx);
    void reset(int rate);
    void update() override;
    void setScale(int Scale);
    void mix(Mixer &m);
};

#endif //PAQ8PX_STATIONARYMAP_HPP
