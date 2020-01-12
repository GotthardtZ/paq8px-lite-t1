#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "utils.hpp"
#include "Mixer.hpp"
#include "SimdMixer.hpp"
#include "Shared.hpp"

class MixerFactory {
private:
    Shared *shared = Shared::getInstance();
public:
    Mixer *createMixer(int n, int m, int s);
};

#endif //PAQ8PX_MIXERFACTORY_HPP
