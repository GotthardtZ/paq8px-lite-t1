#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "utils.hpp"
#include "Mixer.hpp"
#include "Shared.hpp"
#include "SimdMixer.hpp"

class MixerFactory {
public:
    auto createMixer(const Shared* const sh, int n, int m, int s) -> Mixer *;
};

#endif //PAQ8PX_MIXERFACTORY_HPP
