#ifndef PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP
#define PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP

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
    const Shared *const shared;
    Array<uint16_t> data;
    const uint32_t mask, stride, bTotal;
    uint32_t context, bCount, B;
    uint16_t *cp;
    const int rate;
    int scale;

public:
    SmallStationaryContextMap(const Shared *const sh, const int bitsOfContext, const int inputBits, const int rate, const int scale)
            : shared(sh), data((UINT64_C(1) << bitsOfContext) * ((UINT64_C(1) << inputBits) - 1)), mask((1U << bitsOfContext) - 1),
            stride((1U << inputBits) - 1), bTotal(inputBits), rate(rate), scale(scale) {
      assert(inputBits > 0 && inputBits <= 8);
      reset();
      set(0);
    }

    void set(uint32_t ctx) {
      context = (ctx & mask) * stride;
      bCount = B = 0;
    }

    void reset() {
      for( uint32_t i = 0; i < data.size(); ++i )
        data[i] = 0x7FFF;
      cp = &data[0];
    }

    void update() override {
      INJECT_SHARED_y
      *cp += ((y << 16U) - (*cp) + (1 << (rate - 1))) >> rate;
      B += (y && B > 0);
    }

    void setScale(const int Scale) { scale = Scale; }

    void mix(Mixer &m) {
      updater.subscribe(this);
      cp = &data[context + B];
      const int prediction = (*cp) >> 4U;
      m.add((stretch(prediction) * scale) >> 8);
      m.add(((prediction - 2048) * scale) >> 9);
      bCount++;
      B += B + 1;
      assert(bCount <= bTotal);
    }
};

#endif //PAQ8PX_SMALLSTATIONARYCONTEXTMAP_HPP
