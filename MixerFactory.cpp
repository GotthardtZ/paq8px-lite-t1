#include "MixerFactory.hpp"

void MixerFactory::setSimd(SIMD simd) {
  chosenSimd = simd;
}

Mixer *MixerFactory::createMixer(const int n, const int m, const int s) {
  if( chosenSimd == SIMD_NONE )
    return new SIMDMixer<SIMD_NONE>(n, m, s);
  if( chosenSimd == SIMD_SSE2 )
    return new SIMDMixer<SIMD_SSE2>(n, m, s);
  if( chosenSimd == SIMD_AVX2 )
    return new SIMDMixer<SIMD_AVX2>(n, m, s);
  assert(false);
  return nullptr;
}
