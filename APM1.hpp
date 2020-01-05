#ifndef PAQ8PX_APM1_HPP
#define PAQ8PX_APM1_HPP

#include <cstdint>
#include <cassert>

/**
 * APM1 maps a probability and a context into a new probability that bit y will next be 1.
 * After each guess it updates its state to improve future guesses.
 * Probabilities are scaled 12 bits (0-4095).
 */
class APM1 : IPredictor {
private:
    const Shared *const shared;
    int index; // last p, context
    const int n; // number of contexts
    Array<uint16_t> t; // [n][33]:  p, context -> p
    const int rate;

public:
    /**
     * APM1 a(sh, n, r) creates an instance with n contexts and learning rate r.
     * rate determines the learning rate. smaller = faster, must be in the range (0, 32).
     * Uses 66*n bytes memory.
     * @param sh
     * @param n the number of contexts
     * @param r the learning rate
     */
    APM1(const Shared *sh, int n, int r);
    /**
     * a.p(pr, cx) returns adjusted probability in context cx (0 to n-1).
     * @param pr initial (pre-adjusted) probability
     * @param cxt the context
     * @return adjusted probability
     */
    int p(int pr, int cxt);
    void update() override;
};

APM1::APM1(const Shared *const sh, const int n, const int r) : shared(sh), index(0), n(n), t(n * 33), rate(r) {
  assert(n > 0 && rate > 0 && rate < 32);
  // maps p, cxt -> p initially
  for( int i = 0; i < n; ++i )
    for( int j = 0; j < 33; ++j )
      if( i == 0 ) {
        t[i * 33 + j] = squash((j - 16) * 128) * 16;
      } else {
        t[i * 33 + j] = t[j];
      }
}

int APM1::p(int pr, const int cxt) {
  updater.subscribe(this);
  assert(pr >= 0 && pr < 4096 && cxt >= 0 && cxt < n);
  pr = stretch(pr);
  const int w = pr & 127U; // interpolation weight (33 points)
  index = ((pr + 2048) >> 7) + cxt * 33;
  return (t[index] * (128 - w) + t[index + 1] * w) >> 11U;
}

void APM1::update() {
  INJECT_SHARED_y
  const int g = (y << 16U) + (y << rate) - y - y;
  t[index] += (g - t[index]) >> rate;
  t[index + 1] += (g - t[index + 1]) >> rate;
}

#endif //PAQ8PX_APM1_HPP
