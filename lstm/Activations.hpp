#ifndef PAQ8PX_ACTIVATIONS_HPP
#define PAQ8PX_ACTIVATIONS_HPP

#include "../utils.hpp"
#include "SimdFunctions.hpp"
#include <cmath>

template <typename T>
struct is_valid_activation {
  enum { value = (std::is_same<typename std::result_of<T(float&)>::type, void>::value == std::true_type::value) };
};

struct Tanh {
  ALWAYS_INLINE void operator() (float& v) const { v = tanha(v); }
};

struct Logistic {
  //the following is equivalent to the sigmoid (v = 1.f / (1.f + exp(-v));)
  //(but faster because tanh is faster): 
  ALWAYS_INLINE void operator() (float& v) const { v = (tanha(v * 0.5f) + 1.0f) * 0.5f; }
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
