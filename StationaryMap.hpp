#ifndef PAQ8PX_STATIONARYMAP_HPP
#define PAQ8PX_STATIONARYMAP_HPP

/*
  map for modelling contexts of (nearly-)stationary data.
  The context is looked up directly. For each bit modelled, a 32bit element stores
  a 22 bit prediction and a 10 bit adaptation rate offset.

  - BitsOfContext: How many bits to use for each context. Higher bits are discarded.
  - InputBits: How many bits [1..8] of input are to be modelled for each context.
    New contexts must be set at those intervals.
  - Rate: Initial adaptation rate offset [0..1023]. Lower offsets mean faster adaptation.
    Will be increased on every occurrence until the higher bound is reached.

    Uses (2^(BitsOfContext+2))*((2^InputBits)-1) bytes of memory.
*/

class StationaryMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 2;

private:
    const Shared *const shared;
    Array<uint32_t> data;
    const uint32_t mask, maskBits, stride, bTotal;
    uint32_t context, bCount, B;
    uint32_t *cp;
    int scale;
    const uint16_t limit;
    int *dt;

public:
    StationaryMap(const Shared *const sh, const int bitsOfContext, const int inputBits, const int scale,
                  const uint16_t limit) : shared(sh),
                                          data((UINT64_C(1) << bitsOfContext) * ((UINT64_C(1) << inputBits) - 1)),
                                          mask((1U << bitsOfContext) - 1), maskBits(bitsOfContext),
                                          stride((1U << inputBits) - 1), bTotal(inputBits), scale(scale), limit(limit) {
      assert(inputBits > 0 && inputBits <= 8);
      assert(bitsOfContext + inputBits <= 24);
      dt = DivisionTable::getDT();
      reset(0);
      set(0);
    }

    void set_direct(uint32_t ctx) { // ctx must be a direct context (no hash)
      context = (ctx & mask) * stride;
      bCount = B = 0;
    }

    void set(uint64_t ctx) { // ctx must be a hash
      context = (finalize64(ctx, maskBits) & mask) * stride;
      bCount = B = 0;
    }

    void reset(const int rate) {
      for( uint32_t i = 0; i < data.size(); ++i )
        data[i] = (0x7FF << 20) | min(1023, rate);
      cp = &data[0];
    }

    void update() override {
      INJECT_SHARED_y
      uint32_t count = min(min(limit, 0x3FF), ((*cp) & 0x3FFU) + 1);
      int prediction = (*cp) >> 10, error = (y << 22U) - prediction;
      error = ((error / 8) * dt[count]) / 1024;
      prediction = min(0x3FFFFF, max(0, prediction + error));
      *cp = (prediction << 10U) | count;
      B += (y && B > 0);
    }

    void setScale(const int Scale) { scale = Scale; }

    void mix(Mixer &m) {
      updater.subscribe(this);
      cp = &data[context + B];
      int prediction = (*cp) >> 20;
      m.add((stretch(prediction) * scale) >> 8);
      m.add(((prediction - 2048) * scale) >> 9);
      bCount++;
      B += B + 1;
      assert(bCount <= bTotal);
    }
};

#endif //PAQ8PX_STATIONARYMAP_HPP
