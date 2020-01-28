#include "Mixer.hpp"
#include "utils.hpp"

Mixer::Mixer(const int n, const int m, const int s) : n(n), m(m), s(s), scaleFactor(0), tx(n), wx(n * m), cxt(s), info(s), rates(s), pr(s) {
#ifndef NDEBUG
  printf("Created Mixer with n = %d, m = %d, s = %d\n", n, m, s);
#endif
  for( uint64_t i = 0; i < s; ++i ) {
    pr[i] = 2048; //initial p=0.5
    rates[i] = DEFAULT_LEARNING_RATE;
    info[i].reset();
  }
  reset();
}

void Mixer::add(const int x) {
#ifndef NDEBUG
  printf("Mixer::add(%d)\n", x);
#endif
  assert(nx < n);
  assert(x == short(x));
  tx[nx++] = static_cast<short>(x);
}

void Mixer::set(const uint32_t cx, const uint32_t range, const int rate) {
#ifndef NDEBUG
  printf("Mixer::set(%d, %d, %d)\n", cx, range, rate);
#endif
  assert(numContexts < s);
  assert(cx < range);
  assert(base + range <= m);
  if((shared->options & OPTION_ADAPTIVE) == 0U ) {
    rates[numContexts] = rate;
  }
  cxt[numContexts++] = base + cx;
  base += range;
//#ifndef NDEBUG
//  printf("numContexts: %d base: %d\n", numContexts, range); //for debugging: how many input sets do we have?
//#endif
}

void Mixer::reset() {
  nx = 0;
  base = 0;
  numContexts = 0;
}
