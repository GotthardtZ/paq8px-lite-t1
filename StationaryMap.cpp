#include "StationaryMap.hpp"

StationaryMap::StationaryMap(const Shared* const sh, const int bitsOfContext, const int inputBits, const int scale, const uint16_t limit) : 
  shared(sh),
  data((1ULL << bitsOfContext) * ((1ULL << inputBits) - 1)),
  mask((1 << bitsOfContext) - 1), 
  maskBits(bitsOfContext),
  stride((1 << inputBits) - 1), bTotal(inputBits), scale(scale), limit(limit) {
#ifdef VERBOSE
  printf("Created StationaryMap with bitsOfContext = %d, inputBits = %d, scale = %d, limit = %d\n", bitsOfContext, inputBits, scale, limit);
#endif
  assert(inputBits > 0 && inputBits <= 8);
  assert(bitsOfContext + inputBits <= 24);
  dt = DivisionTable::getDT();
  reset(0);
  set(0);
}

void StationaryMap::setDirect(uint32_t ctx) {
  context = (ctx & mask) * stride;
  bCount = b = 0;
}

void StationaryMap::set(uint64_t ctx) {
  context = (finalize64(ctx, maskBits) & mask) * stride;
  bCount = b = 0;
}

void StationaryMap::setscale(int scale) {
  this->scale = scale;
}

void StationaryMap::reset(const int rate) {
  for( uint32_t i = 0; i < data.size(); ++i ) {
    data[i] = (0x7FF << 20) | min(1023, rate);
  }
  cp = &data[0];
}

void StationaryMap::update() {
  INJECT_SHARED_y
  uint32_t count = min(min(limit, 0x3FF), ((*cp) & 0x3FF) + 1);
  int prediction = (*cp) >> 10; // 22 bit p
  int error = (y << 22) - prediction; // 22 bit error
  error = ((error / 8) * dt[count]) / 1024; // error = error *  1.0/(count+1.5)
  prediction = min(0x3FFFFF, max(0, prediction + error));
  *cp = (prediction << 10) | count;
  b += static_cast<uint32_t>((y != 0) && b > 0);
}

void StationaryMap::mix(Mixer &m) {
  shared->GetUpdateBroadcaster()->subscribe(this);
  cp = &data[context + b];
  const int prediction = (*cp) >> 20;
  m.add((stretch(prediction) * scale) >> 8);
  m.add(((prediction - 2048) * scale) >> 9);
  bCount++;
  b += b + 1;
  assert(bCount <= bTotal);
}
