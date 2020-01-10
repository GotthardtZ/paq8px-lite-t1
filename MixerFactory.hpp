#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "utils.hpp"
#include "Mixer.hpp"
#include "SimdMixer.hpp"

class MixerFactory {
public:
    static void setSimd(SIMD simd) {
      chosenSimd = simd;
    }

    static Mixer *createMixer(const int n, const int m, const int s) {
      if( chosenSimd == SIMD_NONE )
        return new SIMDMixer<SIMD_NONE>(n, m, s);
      if( chosenSimd == SIMD_SSE2 )
        return new SIMDMixer<SIMD_SSE2>(n, m, s);
      if( chosenSimd == SIMD_AVX2 )
        return new SIMDMixer<SIMD_AVX2>(n, m, s);
      assert(false);
      return nullptr;
    }
};

#endif //PAQ8PX_MIXERFACTORY_HPP
