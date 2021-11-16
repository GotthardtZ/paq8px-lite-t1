#ifndef PAQ8PX_PREDICTOR_HPP
#define PAQ8PX_PREDICTOR_HPP

#include "Shared.hpp"
#include "Mixer.hpp"
#include "MixerFactory.hpp"
#include "model/NormalModel.hpp"


/**
 * A Predictor estimates the probability that the next bit of uncompressed data is 1.
 */
class Predictor {
private:
    Shared *shared;
    MixerFactory mixerFactory;
    Mixer* m;

public:
  NormalModel normalModel;
  Predictor(Shared* const sh);
  void Update();
  uint32_t p();

};

#endif //PAQ8PX_PREDICTOR_HPP
