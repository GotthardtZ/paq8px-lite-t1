#ifndef PAQ8PX_LMS_HPP
#define PAQ8PX_LMS_HPP

#include <cstdint>
#include <cstdio>
#include <cassert>
#include "Ilog.hpp"

///////////////// Least Mean Squares predictor /////////////////

template<typename F, typename T>
class LMS {
private:
    F *weights, *eg, *buffer;
    F rates[2];
    F rho, complement, eps, prediction;
    int s, d;

public:
    LMS(const int s, const int d, const F lRate, const F rRate, const F rho = (F) 0.95, const F eps = (F) 1e-3) : rates {lRate, rRate},
            rho(rho), complement(1.0f - rho), eps(eps), prediction(0.0f), s(s), d(d) {
      assert(s > 0 && d > 0);
      weights = new F[s + d], eg = new F[s + d], buffer = new F[s + d];
      reset();
    }

    ~LMS() {
      delete weights, delete eg, delete buffer;
    }

    F predict(const T sample) {
      memmove(&buffer[s + 1], &buffer[s], (d - 1) * sizeof(F));
      buffer[s] = sample;
      prediction = 0.;
      for( int i = 0; i < s + d; i++ )
        prediction += weights[i] * buffer[i];
      return prediction;
    }

    void update(const T sample) {
      const F error = sample - prediction;
      int i = 0;
      for( ; i < s; i++ ) {
        const F gradient = error * buffer[i];
        eg[i] = rho * eg[i] + complement * (gradient * gradient);
        weights[i] += (rates[0] * gradient * rsqrt(eg[i] + eps));
      }
      for( ; i < s + d; i++ ) {
        const F gradient = error * buffer[i];
        eg[i] = rho * eg[i] + complement * (gradient * gradient);
        weights[i] += (rates[1] * gradient * rsqrt(eg[i] + eps));
      }
      memmove(&buffer[1], &buffer[0], (s - 1) * sizeof(F));
      buffer[0] = sample;
    }

    void reset() {
      for( int i = 0; i < s + d; i++ )
        weights[i] = eg[i] = buffer[i] = 0.;
    }
};

#endif //PAQ8PX_LMS_HPP
