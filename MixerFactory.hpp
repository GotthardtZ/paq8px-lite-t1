#ifndef PAQ8PX_MIXERFACTORY_HPP
#define PAQ8PX_MIXERFACTORY_HPP

#include "Utils.hpp"
#include "Mixer.hpp"
#include "Shared.hpp"
#include "SimdMixer.hpp"

class MixerFactory {
private:
  const Shared* const shared;
public:
  Mixer* createMixer(int n, int m, int s) const;
  MixerFactory(const Shared* const sh);
};

#endif //PAQ8PX_MIXERFACTORY_HPP
