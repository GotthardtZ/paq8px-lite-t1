#include "MixerFactory.hpp"

auto MixerFactory::createMixer(const int n, const int m, const int s) -> Mixer * {
  if( shared->chosenSimd == SIMD_NONE ) {
    return new SIMDMixer<SIMD_NONE>(n, m, s);
  }
  else if( shared->chosenSimd == SIMD_SSE2 ) {
    return new SIMDMixer<SIMD_SSE2>(n, m, s);
  }
  else if (shared->chosenSimd == SIMD_SSSE3) {
      return new SIMDMixer<SIMD_SSSE3>(n, m, s);
  }
  else if( shared->chosenSimd == SIMD_AVX2 ) {
    return new SIMDMixer<SIMD_AVX2>(n, m, s);
  }
  else if (shared->chosenSimd == SIMD_NEON) {
    return new SIMDMixer<SIMD_NEON>(n, m, s);
  }
  assert(false);
  return nullptr;
}
