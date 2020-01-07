#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

class MixerFactory {
public:
    static void setSimd(SIMD simd) {
      chosenSimd = simd;
    }

    static Mixer *createMixer(const Shared *const sh, const int n, const int m, const int s) {
      if( chosenSimd == SIMD_NONE )
        return new SIMDMixer<SIMD_NONE>(sh, n, m, s);
      if( chosenSimd == SIMD_SSE2 )
        return new SIMDMixer<SIMD_SSE2>(sh, n, m, s);
      if( chosenSimd == SIMD_AVX2 )
        return new SIMDMixer<SIMD_AVX2>(sh, n, m, s);
      assert(false);
      return nullptr;
    }
};

#endif //PAQ8PX_MIXERFACTORY_HPP
