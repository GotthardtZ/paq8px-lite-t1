#ifndef PAQ8PX_INDIRECTMAP_HPP
#define PAQ8PX_INDIRECTMAP_HPP

class IndirectMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 2;

private:
    Shared *shared = Shared::getInstance();
    Random rnd;
    Array<uint8_t> data;
    StateMap sm;
    const uint32_t mask, maskBits, stride, bTotal;
    uint32_t context, bCount, b;
    uint8_t *cp;
    int scale;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    IndirectMap(const int bitsOfContext, const int inputBits, const int scale, const int limit) : data(
            (UINT64_C(1) << bitsOfContext) * ((UINT64_C(1) << inputBits) - 1)),
            sm {1, 256, 1023, StateMap::BitHistory}, /* StateMap : s, n, lim, init */
            mask((1U << bitsOfContext) - 1), maskBits(bitsOfContext), stride((1U << inputBits) - 1), bTotal(inputBits), scale(scale) {
      assert(inputBits > 0 && inputBits <= 8);
      assert(bitsOfContext + inputBits <= 24);
      cp = &data[0];
      setDirect(0);
      sm.setLimit(limit);
    }

    void setDirect(const uint32_t ctx) {
      context = (ctx & mask) * stride;
      bCount = b = 0;
    }

    void set(const uint64_t ctx) {
      context = (finalize64(ctx, maskBits)) * stride;
      bCount = b = 0;
    }

    void update() override {
      INJECT_SHARED_y
      StateTable::update(cp, y, rnd);
      b += (y && b > 0);
    }

    void setScale(const int Scale) { this->scale = Scale; }

    void mix(Mixer &m) {
      updater->subscribe(this);
      cp = &data[context + b];
      const uint8_t state = *cp;
      const int p1 = sm.p1(state);
      m.add((stretch(p1) * scale) >> 8U);
      m.add(((p1 - 2048) * scale) >> 9U);
      bCount++;
      b += b + 1;
      assert(bCount <= bTotal);
    }
};

#endif //PAQ8PX_INDIRECTMAP_HPP
