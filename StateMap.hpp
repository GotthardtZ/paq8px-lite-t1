#ifndef PAQ8PX_STATEMAP_HPP
#define PAQ8PX_STATEMAP_HPP

//////////////////////////// StateMap, APM //////////////////////////

// A StateMap maps a context to a probability.  Methods:
//
// Statemap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
// sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
//     that the next y=1, updating the previous prediction with y (0..1).
//     limit (1..1023, default 1023) is the maximum count for computing a
//     prediction.  Larger values are better for stationary sources.

// This class provides a static (common) 1024-element lookup table for integer division
// Initialization will run multiple times, but the table is created only once
class DivisionTable {
public:
    static int *getDT() {
      static int dt[1024]; // i -> 16K/(i+i+3)
      for( int i = 0; i < 1024; ++i )
        dt[i] = 16384 / (i + i + 3);
      return dt;
    }
};

// This is the base class for StateMap and APM
// Purpose: common members are here
class AdaptiveMap : protected IPredictor {
protected:
    const Shared *const shared;
    Array<uint32_t> t; // cxt -> prediction in high 22 bits, count in low 10 bits
    int limit;
    int *dt; // Pointer to division table
    AdaptiveMap(const Shared *const sh, const int n, const int lim) : shared(sh), t(n), limit(lim) {
      dt = DivisionTable::getDT();
    }

    ~AdaptiveMap() override = default;;

    void update(uint32_t *const p) {
      uint32_t p0 = p[0];
      const int n = p0 & 1023U; //count
      const int pr = p0 >> 10U; //prediction (22-bit fractional part)
      if( n < limit )
        ++p0;
      else
        p0 = (p0 & 0xfffffc00U) | limit;
      INJECT_SHARED_y
      const int target = y << 22U; //(22-bit fractional part)
      const int delta =
              ((target - pr) >> 3U) * dt[n]; //the larger the count (n) the less it should adapt pr+=(target-pr)/(n+1.5)
      p0 += delta & 0xfffffc00U;
      p[0] = p0;
    }

public:
    void setLimit(const int lim) { limit = lim; };
};

class StateMap : public AdaptiveMap {
protected:
    const uint32_t S; // Number of context sets
    const uint32_t N; // Number of contexts in each context set
    uint32_t ncxt; // Number of context indexes present in cxt array (0..s-1)
    Array<uint32_t> cxt; // context index of last prediction per context set
public:
    enum MAPTYPE {
        GENERIC, BIT_HISTORY, RUN
    };

    StateMap(const Shared *const sh, const int s, const int n, const int lim, const MAPTYPE mapType) : AdaptiveMap(sh,
                                                                                                                   n *
                                                                                                                   s,
                                                                                                                   lim),
                                                                                                       S(s), N(n),
                                                                                                       ncxt(0), cxt(s) {
      assert(S > 0 && N > 0);
      assert(limit > 0 && limit < 1024);
      if( mapType == BIT_HISTORY ) { // when the context is a bit history byte, we have a-priory for p
        assert((N & 255) == 0);
        for( uint32_t cx = 0; cx < N; ++cx ) {
          auto state = uint8_t(cx & 255U);
          uint32_t n0 = StateTable::next(state, 2) * 3 + 1;
          uint32_t n1 = StateTable::next(state, 3) * 3 + 1;
          for( uint32_t s = 0; s < S; ++s )
            t[s * N + cx] = ((n1 << 20) / (n0 + n1)) << 12U;
        }
      } else if( mapType == RUN ) { // when the context is a run count: we have a-priory for p
        for( uint32_t cx = 0; cx < N; ++cx ) {
          const int predictedBit = (cx) & 1;
          const int uncertainty = (cx >> 1) & 1;
          //const int bp = (cx>>2)&1; // unused in calculation - a-priory does not seem to depend on bitPosition in the general case
          const int runCount = (cx >> 4); // 0..254
          uint32_t n0 = uncertainty * 16 + 16;
          uint32_t n1 = runCount * 128 + 16;
          if( predictedBit == 0 )
            std::swap(n0, n1);
          for( uint32_t s = 0; s < S; ++s )
            t[s * N + cx] = ((n1 << 20) / (n0 + n1)) << 12 | min(runCount, limit);
        }
      } else { // no a-priory
        for( uint32_t i = 0; i < N * S; ++i )
          t[i] = (1u << 31) + 0; //initial p=0.5, initial count=0
      }
    }

    void reset(const int rate) {
      for( uint32_t i = 0; i < N * S; ++i )
        t[i] = (t[i] & 0xfffffc00) | min(rate, t[i] & 0x3FF);
    }

    void update() override {
      assert(ncxt <= S);
      while( ncxt > 0 ) {
        ncxt--;
        const uint32_t idx = cxt[ncxt];
        if( idx + 1 == 0 )
          continue; // UINT32_MAX: skipped context
        assert(ncxt * N <= idx && idx < (ncxt + 1) * N);
        AdaptiveMap::update(&t[idx]);
      }
    }

    //call p1() when there is only 1 context set
    //no need to call subscribe()
    int p1(const uint32_t cx) {
      updater.subscribe(this);
      assert(cx >= 0 && cx < N);
      cxt[0] = cx;
      ncxt++;
      return t[cx] >> 20;
    }

    //call p2() for each context when there is more context sets
    //and call subscribe() once
    int p2(const uint32_t s, const uint32_t cx) {
      assert(s >= 0 && s < S);
      assert(cx >= 0 && cx < N);
      assert(s == ncxt);
      const uint32_t idx = ncxt * N + cx;
      cxt[ncxt] = idx;
      ncxt++;
      return t[idx] >> 20;
    }

    void subscribe() {
      updater.subscribe(this);
    }

    //call skip() instead of p2() when the context is unknown or uninteresting
    //remark: no need to call skip() when there is only 1 context or all contexts will be skipped
    void skip(const uint32_t s) {
      assert(s >= 0 && s < S);
      assert(s == ncxt);
      cxt[ncxt] = 0 - 1; // UINT32_MAX: mark for skipping
      ncxt++;
    }

    void print() const {
      for( uint32_t i = 0; i < t.size(); i++ ) {
        uint32_t p0 = t[i] >> 10;
        printf("%d\t%d\n", i, p0);
      }
    }
};


// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n,STEPS) creates with n contexts using 4*STEPS*n bytes memory.
// a.update() updates probability map. y=(0..1) is the last bit
// a.p(pr, cx, limit) returns a new probability (0..4095) like with StateMap.
//     cx=(0..n-1) is the context.
//     pr=(0..4095) is considered part of the context.
//     The output is computed by interpolating pr into STEPS ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     limit=(0..1023): set a lower limit (like 255) for faster adaptation.

class APM : public AdaptiveMap {
private:
    const int N; // Number of contexts
    const int steps;
    int cxt; // context index of last prediction
public:
    APM(const Shared *const sh, const int n, const int s) : AdaptiveMap(sh, n * s, 1023), N(n * s), steps(s), cxt(0) {
      assert(s > 4); // number of steps - must be a positive integer bigger than 4
      for( int i = 0; i < N; ++i ) {
        int p = ((i % steps * 2 + 1) * 4096) / (steps * 2) - 2048;
        t[i] = (uint32_t(squash(p)) << 20) + 6; //initial count: 6
      }
    }

    void update() override {
      assert(cxt >= 0 && cxt < N);
      AdaptiveMap::update(&t[cxt]);
    }

    int p(int pr, int cx, const int lim) {
      updater.subscribe(this);
      assert(pr >= 0 && pr < 4096);
      assert(cx >= 0 && cx < N / steps);
      assert(limit > 0 && limit < 1024);
      AdaptiveMap::setLimit(lim);
      pr = (stretch(pr) + 2048) * (steps - 1);
      int wt = pr & 0xfff; // interpolation weight (0..4095)
      cx = cx * steps + (pr >> 12);
      assert(cx >= 0 && cx < N - 1);
      cxt = cx + (wt >> 11);
      pr = ((t[cx] >> 13) * (4096 - wt) + (t[cx + 1] >> 13) * wt) >> 19;
      return pr;
    }
};

#endif //PAQ8PX_STATEMAP_HPP
