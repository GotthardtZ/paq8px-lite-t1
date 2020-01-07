#ifndef PAQ8PX_APM_HPP
#define PAQ8PX_APM_HPP

#include "AdaptiveMap.hpp"

/**
 * An APM maps a probability and a context to a new probability.
 */
class APM : public AdaptiveMap {
private:
    const int N; // Number of contexts
    const int steps;
    int cxt; // context index of last prediction
public:
    /**
     * APM a(n,STEPS) creates with n contexts using 4*STEPS*n bytes memory.
     * @param sh
     * @param n
     * @param s
     */
    APM(const Shared *const sh, const int n, const int s) : AdaptiveMap(sh, n * s, 1023), N(n * s), steps(s), cxt(0) {
      assert(s > 4); // number of steps - must be a positive integer bigger than 4
      for( int i = 0; i < N; ++i ) {
        int p = ((i % steps * 2 + 1) * 4096) / (steps * 2) - 2048;
        t[i] = (uint32_t(squash(p)) << 20) + 6; //initial count: 6
      }
    }

    /**
     * a.update() updates probability map. y=(0..1) is the last bit
     */
    void update() override {
      assert(cxt >= 0 && cxt < N);
      AdaptiveMap::update(&t[cxt]);
    }

    /**
     * a.p(pr, cx, limit) returns a new probability (0..4095) like with StateMap.
     * cx=(0..n-1) is the context.
     * pr=(0..4095) is considered part of the context.
     * The output is computed by interpolating pr into STEPS ranges nonlinearly
     * with smaller ranges near the ends.  The initial output is pr.
     * limit=(0..1023): set a lower limit (like 255) for faster adaptation.
     * @param pr
     * @param cx
     * @param lim
     * @return
     */
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

#endif //PAQ8PX_APM_HPP
