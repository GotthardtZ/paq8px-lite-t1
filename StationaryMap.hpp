#ifndef PAQ8PX_STATIONARYMAP_HPP
#define PAQ8PX_STATIONARYMAP_HPP

#include "IPredictor.hpp"
#include "DivisionTable.hpp"
#include "Hash.hpp"
#include "Mixer.hpp"
#include "Stretch.hpp"
#include "UpdateBroadcaster.hpp"

/**
 * Map for modelling contexts of (nearly-)stationary data.
 * The context is looked up directly. For each bit modelled, a 32bit element stores
 * a 22 bit prediction and a 10 bit adaptation rate offset.
 *
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
    uint32_t context {};
    uint32_t bCount {};
    uint32_t b {};
    uint32_t *cp {};
    int scale;
    const uint16_t limit;
    int *dt;

public:
    /**
     * Construct using (2^(BitsOfContext+2))*((2^InputBits)-1) bytes of memory.
     * @param bitsOfContext How many bits to use for each context. Higher bits are discarded.
     * @param inputBits How many bits [1..8] of input are to be modelled for each context. New contexts must be set at those intervals.
     * @param scale
     * @param limit
     */
    StationaryMap(int bitsOfContext, int inputBits, int scale, uint16_t limit);

    /**
     * ctx must be a direct context (no hash)
     * @param ctx
     */
    void setDirect(uint32_t ctx);

    /**
     * ctx must be a hash
     * @param ctx
     */
    void set(uint64_t ctx);
    void reset(int rate);
    void update() override;
    void mix(Mixer &m);
};

#endif //PAQ8PX_STATIONARYMAP_HPP
