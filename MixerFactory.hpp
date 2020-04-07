#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "utils.hpp"
#include "Mixer.hpp"
#include "Shared.hpp"
#include "SimdMixer.hpp"

class MixerFactory {
private:
    Shared *shared = Shared::getInstance();
public:
    auto createMixer(int n, int m, int s) -> Mixer *;
};

#endif //PAQ8PX_MIXERFACTORY_HPP
