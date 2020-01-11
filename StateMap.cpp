#include "StateMap.hpp"

StateMap::StateMap(const int s, const int n, const int lim, const StateMap::MAPTYPE mapType) : AdaptiveMap(n * s, lim), S(s), N(n), numContexts(0), cxt(s) {
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

void StateMap::reset(const int rate) {
  for( uint32_t i = 0; i < N * S; ++i )
    t[i] = (t[i] & 0xfffffc00u) | min(rate, t[i] & 0x3FFU);
}

void StateMap::update() {
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

int StateMap::p1(const uint32_t cx) {
  updater->subscribe(this);
  assert(cx >= 0 && cx < N);
  cxt[0] = cx;
  numContexts++;
  return t[cx] >> 20U;
}

int StateMap::p2(const uint32_t s, const uint32_t cx) {
  assert(s >= 0 && s < S);
  assert(cx >= 0 && cx < N);
  assert(s == numContexts);
  const uint32_t idx = numContexts * N + cx;
  cxt[numContexts] = idx;
  numContexts++;
  return t[idx] >> 20U;
}

void StateMap::subscribe() {
  updater->subscribe(this);
}

void StateMap::skip(const uint32_t s) {
  assert(s >= 0 && s < S);
  assert(s == numContexts);
  cxt[numContexts] = 0 - 1; // UINT32_MAX: mark for skipping
  numContexts++;
}

void StateMap::print() const {
  for( uint32_t i = 0; i < t.size(); i++ ) {
    uint32_t p0 = t[i] >> 10;
    printf("%d\t%d\n", i, p0);
  }
}
