#ifndef PAQ8PX_ACTIVATIONS_HPP
#define PAQ8PX_ACTIVATIONS_HPP

#include "../utils.hpp"
#include <cmath>

template <typename T>
struct is_valid_activation {
  enum { value = (std::is_same<typename std::result_of<T(float&)>::type, void>::value == std::true_type::value) };
};

struct Tanh {
  ALWAYS_INLINE void operator() (float& v) const { v = std::tanh(v); }
};

struct Logistic {
  ALWAYS_INLINE void operator() (float& v) const { v = 1.f / (1.f + std::exp(-v)); }
};

template <std::uint8_t Slope = 2>
struct HardSigmoid {
  static_assert(Slope > 0, "Slope denominator must be > 0");
private:
  static constexpr float mul = 1.f / Slope;
public:
  ALWAYS_INLINE void operator() (float& v) const { v = std::max<float>(0.f, std::min<float>(1.f, v * mul + 0.5f)); }
};

struct HardTanh {
  ALWAYS_INLINE void operator() (float& v) const { v = std::max<float>(-1.f, std::min<float>(1.f, v)); }
};

#endif //PAQ8PX_ACTIVATIONS_HPP
