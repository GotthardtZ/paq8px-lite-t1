#include "StationaryMap.hpp"

StationaryMap::StationaryMap(const Shared* const sh, const int bitsOfContext, const int inputBits, const int scale, const uint16_t limit) : 
  shared(sh),
  data((1ULL << bitsOfContext) * ((1ULL << inputBits) - 1)),
  mask((1U << bitsOfContext) - 1), 
  maskBits(bitsOfContext),
  stride((1U << inputBits) - 1), bTotal(inputBits), scale(scale), limit(limit) {
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

void StationaryMap::reset(const int rate) {
  for( uint32_t i = 0; i < data.size(); ++i ) {
    data[i] = (0x7FFU << 20U) | min(1023, rate);
  }
  cp = &data[0];
}

void StationaryMap::update() {
  INJECT_SHARED_y
  uint32_t count = min(min(limit, 0x3FF), ((*cp) & 0x3FFU) + 1);
  int prediction = (*cp) >> 10U;
  int error = (y << 22U) - prediction;
  error = ((error / 8) * dt[count]) / 1024;
  prediction = min(0x3FFFFF, max(0, prediction + error));
  *cp = (prediction << 10U) | count;
  b += static_cast<uint32_t>((y != 0U) && b > 0);
}

void StationaryMap::mix(Mixer &m) {
  shared->GetUpdateBroadcaster()->subscribe(this);
  cp = &data[context + b];
  int prediction = (*cp) >> 20U;
  m.add((stretch(prediction) * scale) >> 8U);
  m.add(((prediction - 2048) * scale) >> 9U);
  bCount++;
  b += b + 1;
  assert(bCount <= bTotal);
}
