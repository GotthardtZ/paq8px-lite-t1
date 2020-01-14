#include "Mixer.hpp"
#include "utils.hpp"

Mixer::Mixer(const int n, const int m, const int s) : n(n), m(m), s(s), scaleFactor(0), tx(n), wx(n * m), cxt(s), info(s), rates(s), pr(s) {
  for( uint64_t i = 0; i < s; ++i ) {
    pr[i] = 2048; //initial p=0.5
    rates[i] = DEFAULT_LEARNING_RATE;
    info[i].reset();
  }
  reset();
}

void Mixer::add(const int x) {
  assert(nx < n);
  assert(x == short(x));
  tx[nx++] = (short) x;
}

void Mixer::set(const uint32_t cx, const uint32_t range, const int rate) {
  assert(numContexts < s);
  assert(cx < range);
  assert(base + range <= m);
  if( (shared->options & OPTION_ADAPTIVE) == 0U)
    rates[numContexts] = rate;
  cxt[numContexts++] = base + cx;
  base += range;
//#ifndef DNDEBUG
//  printf("numContexts: %d base: %d\n", numContexts, range); //for debugging: how many input sets do we have?
//#endif
}

void Mixer::reset() {
  nx = base = numContexts = 0;
}
