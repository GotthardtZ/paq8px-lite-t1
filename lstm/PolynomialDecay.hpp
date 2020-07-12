#ifndef PAQ8PX_POLYNOMIALDECAY_HPP
#define PAQ8PX_POLYNOMIALDECAY_HPP

#include "IDecay.hpp"
#include "../utils.hpp"
#include <cmath>

template <std::uint16_t LR, std::uint8_t E1, std::uint16_t ELR, std::uint8_t E2, std::uint8_t N, std::uint8_t D, std::uint64_t Steps = 0>
class PolynomialDecay :
  public IDecay {
  static_assert(D > 0, "Polynomial decay denominator must be > 0");
private:
  static constexpr double power = static_cast<double>(N) / D;
  static constexpr double mul = (Steps > 0) ? 1. / static_cast<double>(Steps) : 0.;
  static constexpr float learning_rate = static_cast<float>(static_cast<double>(LR) * neg_pow10<E1>::value);
  static constexpr float end_learning_rate = static_cast<float>(static_cast<double>(ELR) * neg_pow10<E2>::value);
public:
  ALWAYS_INLINE void Apply(float& rate, std::uint64_t const time_step) const {
    if (Steps > 0) {
      if (time_step < Steps)
        rate = (learning_rate - end_learning_rate) * static_cast<float>(std::pow((1. - static_cast<double>(time_step) * mul), power)) + end_learning_rate;
      else
        rate = end_learning_rate / static_cast<float>(std::pow(5e-5 * (time_step - Steps) + 1., power));
    }
    else
      rate = std::max<float>(learning_rate / static_cast<float>(std::pow(5e-5 * time_step + 1., power)), end_learning_rate);
  }
};

#endif //PAQ8PX_POLYNOMIALDECAY_HPP
