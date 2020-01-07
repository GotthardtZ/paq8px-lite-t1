#ifndef PAQ8PX_STATEMAP_HPP
#define PAQ8PX_STATEMAP_HPP

#include "DivisionTable.hpp"
#include "AdaptiveMap.hpp"

/**
 * A StateMap maps a context to a probability.
 */
class StateMap : public AdaptiveMap {
protected:
    const uint32_t S; // Number of context sets
    const uint32_t N; // Number of contexts in each context set
    uint32_t numContexts; // Number of context indexes present in cxt array (0..s-1)
    Array<uint32_t> cxt; // context index of last prediction per context set
public:
    enum MAPTYPE {
        Generic, BitHistory, Run
    };

    /**
     * creates a StateMap with \n contexts using 4*n bytes memory.
     * @param sh
     * @param s
     * @param n number of contexts
     * @param lim
     * @param mapType
     */
    StateMap(const Shared *const sh, const int s, const int n, const int lim, const MAPTYPE mapType) : AdaptiveMap(sh, n * s, lim), S(s),
            N(n), numContexts(0), cxt(s) {
      assert(S > 0 && N > 0);
      assert(limit > 0 && limit < 1024);
      if( mapType == BitHistory ) { // when the context is a bit history byte, we have a-priory for p
        assert((N & 255) == 0);
        for( uint32_t cx = 0; cx < N; ++cx ) {
          auto state = uint8_t(cx & 255U);
          uint32_t n0 = StateTable::next(state, 2) * 3 + 1;
          uint32_t n1 = StateTable::next(state, 3) * 3 + 1;
          for( uint32_t s = 0; s < S; ++s )
            t[s * N + cx] = ((n1 << 20U) / (n0 + n1)) << 12U;
        }
      } else if( mapType == Run ) { // when the context is a run count: we have a-priory for p
        for( uint32_t cx = 0; cx < N; ++cx ) {
          const int predictedBit = (cx) & 1U;
          const int uncertainty = (cx >> 1U) & 1U;
          //const int bp = (cx>>2)&1; // unused in calculation - a-priory does not seem to depend on bitPosition in the general case
          const int runCount = (cx >> 4U); // 0..254
          uint32_t n0 = uncertainty * 16 + 16;
          uint32_t n1 = runCount * 128 + 16;
          if( predictedBit == 0 )
            std::swap(n0, n1);
          for( uint32_t s = 0; s < S; ++s )
            t[s * N + cx] = ((n1 << 20U) / (n0 + n1)) << 12U | min(runCount, limit);
        }
      } else { // no a-priory
        for( uint32_t i = 0; i < N * S; ++i )
          t[i] = (1U << 31U) + 0; //initial p=0.5, initial count=0
      }
    }

    void reset(const int rate) {
      for( uint32_t i = 0; i < N * S; ++i )
        t[i] = (t[i] & 0xfffffc00u) | min(rate, t[i] & 0x3FFU);
    }

    void update() override {
      assert(numContexts <= S);
      while( numContexts > 0 ) {
        numContexts--;
        const uint32_t idx = cxt[numContexts];
        if( idx + 1 == 0 )
          continue; // UINT32_MAX: skipped context
        assert(numContexts * N <= idx && idx < (numContexts + 1) * N);
        AdaptiveMap::update(&t[idx]);
      }
    }

    //call p1() when there is only 1 context set
    //no need to call subscribe()
    int p1(const uint32_t cx) {
      updater.subscribe(this);
      assert(cx >= 0 && cx < N);
      cxt[0] = cx;
      numContexts++;
      return t[cx] >> 20;
    }

    //call p2() for each context when there is more context sets
    //and call subscribe() once
    /**
     * TODO: update this documentation
     * sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
     * that the next y=1, updating the previous prediction with y (0..1).
     * limit (1..1023, default 1023) is the maximum count for computing a
     * prediction.  Larger values are better for stationary sources.
     * @param s
     * @param cx
     * @return
     */
    int p2(const uint32_t s, const uint32_t cx) {
      assert(s >= 0 && s < S);
      assert(cx >= 0 && cx < N);
      assert(s == numContexts);
      const uint32_t idx = numContexts * N + cx;
      cxt[numContexts] = idx;
      numContexts++;
      return t[idx] >> 20U;
    }

    void subscribe() {
      updater.subscribe(this);
    }

    /**
     * Call skip() instead of p2() when the context is unknown or uninteresting.
     * \remark no need to call skip() when there is only 1 context or all contexts will be skipped
     * @param s
     */
    void skip(const uint32_t s) {
      assert(s >= 0 && s < S);
      assert(s == numContexts);
      cxt[numContexts] = 0 - 1; // UINT32_MAX: mark for skipping
      numContexts++;
    }

    void print() const {
      for( uint32_t i = 0; i < t.size(); i++ ) {
        uint32_t p0 = t[i] >> 10;
        printf("%d\t%d\n", i, p0);
      }
    }
};

#include "APM.hpp"

#endif //PAQ8PX_STATEMAP_HPP
