#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "utils.hpp"
#include "Mixer.hpp"
#include "SimdMixer.hpp"

class MixerFactory {
public:
    static void setSimd(SIMD simd);
    static Mixer *createMixer(const int n, const int m, const int s);
};

#endif //PAQ8PX_MIXERFACTORY_HPP
