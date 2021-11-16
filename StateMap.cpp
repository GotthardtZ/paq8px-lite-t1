#include "StateMap.hpp"
#include "Utils.hpp"

StateMap::StateMap(const Shared* const sh, const int n, const int lim, const StateMap::MAPTYPE mapType) :
  shared(sh), numContextsPerSet(n), t(n), limit(lim), cxt(0) {
  assert(limit > 0 && limit < 1024);
  dt = DivisionTable::getDT();
  if( mapType == BitHistory ) { // when the context is a bit history byte, we have a-priory for p
    assert((numContextsPerSet & 255) == 0);
    for( uint64_t cx = 0; cx < numContextsPerSet; ++cx ) {
      const uint8_t state = cx & 255;
      uint32_t n0 = StateTable::next(state, 2);
      uint32_t n1 = StateTable::next(state, 3);
      uint32_t p;
      if (state < 205) {
        n0 = n0 * 3 + 1;
        n1 = n1 * 3 + 1;
        p = ((n1 << 20) / (n0 + n1)) << 12;
        //printf("%d %d\n", state, p >> 20); // verifying
      }
      else if(state < 253){
        int incremental_state = (state - 205)>>2;
        if (((state - 205) & 3) <= 1)
          n0 = 29 + (2 << incremental_state);
        else
          n1 = 29 + (2 << incremental_state);
        n0 = n0 * 3 + 1;
        n1 = n1 * 3 + 1; 
        assert(n0 < 16384 && n1 < 16384);
        p = ((n1 << 18) / (n0 + n1)) << 14 | min((n0 + n1) >> 3, 1023);
        //printf("%d %d\n", state, p >> 20); // verifying
      }
      else { // 253, 254, 255
        p = 2048 << 20; //unused states: p=0.5
      }
      t[cx] = p;
    }
  } else if( mapType == Run ) { // when the context is a run count: we have a-priory for p
    for( uint64_t cx = 0; cx < numContextsPerSet; ++cx ) {
      const int predictedBit = (cx) & 1;
      const int uncertainty = (cx >> 1) & 1;
      const uint32_t runCount = (cx >> 2); // 0..254
      assert(runCount <= 255);
      uint32_t n0 = uncertainty + 1;
      uint32_t n1 = (runCount + 1) * 12;
      if( predictedBit == 0 ) {
        std::swap(n0, n1);
      }
      assert(n0 < 4096 && n1 < 4096);
      t[cx] = ((n1 << 20) / (n0 + n1)) << 12 | limit;
    }
  } else { // no a-priory in the general case
    for( uint32_t i = 0; i < numContextsPerSet; ++i ) {
      t[i] = 2048<<20 | 0; //initial p=0.5, initial count=0
    }
  }
}

void StateMap::update() {
  uint32_t* const p = &t[cxt];
  uint32_t p0 = p[0];
  const int n = p0 & 1023U; //count
  const int pr = p0 >> 10U; //prediction (22-bit fractional part)
  if (n < limit) {
    ++p0;
  }
  else {
    p0 = (p0 & 0xfffffc00U) | limit;
  }
  INJECT_SHARED_y
  const int target = y << 22U; //(22-bit fractional part)
  const int delta = ((target - pr) >> 3U) * dt[n]; //the larger the count (n) the less it should adapt pr+=(target-pr)/(n+1.5)
  p0 += delta & 0xfffffc00U;
  p[0] = p0;
}

auto StateMap::p1(const uint32_t cx) -> int {
  assert(cx >= 0 && cx < numContextsPerSet);
  cxt = cx;
  return t[cx] >> 20;
}


void StateMap::print() const {
  for( uint32_t i = 0; i < t.size(); i++ ) {
    uint32_t p0 = t[i] >> 10;
    uint32_t n = t[i] & 1023;
    printf("%d\t%d\t%d\n", i, p0, n);
  }
}
