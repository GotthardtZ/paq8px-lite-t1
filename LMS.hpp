#ifndef PAQ8PX_LMS_HPP
#define PAQ8PX_LMS_HPP

#include <cstdint>
#include <cstdio>
#include <cassert>

///////////////// Least Mean Squares predictor /////////////////

template<typename F, typename T>
class LMS {
private:
    F *weights, *eg, *buffer;
    F rates[2];
    F rho, complement, eps, prediction;
    int S, D;

public:
    LMS(const int S, const int D, const F lRate, const F rRate, const F rho = (F) 0.95, const F eps = (F) 1e-3) : rates {lRate, rRate},
            rho(rho), complement(1.0f - rho), eps(eps), prediction(0.0f), S(S), D(D) {
      assert(S > 0 && D > 0);
      weights = new F[S + D], eg = new F[S + D], buffer = new F[S + D];
      reset();
    }

    ~LMS() {
      delete weights, delete eg, delete buffer;
    }

    F predict(const T sample) {
      memmove(&buffer[S + 1], &buffer[S], (D - 1) * sizeof(F));
      buffer[S] = sample;
      prediction = 0.;
      for( int i = 0; i < S + D; i++ )
        prediction += weights[i] * buffer[i];
      return prediction;
    }

    void update(const T sample) {
      const F error = sample - prediction;
      int i = 0;
      for( ; i < S; i++ ) {
        const F gradient = error * buffer[i];
        eg[i] = rho * eg[i] + complement * (gradient * gradient);
        weights[i] += (rates[0] * gradient * rsqrt(eg[i] + eps));
      }
      for( ; i < S + D; i++ ) {
        const F gradient = error * buffer[i];
        eg[i] = rho * eg[i] + complement * (gradient * gradient);
        weights[i] += (rates[1] * gradient * rsqrt(eg[i] + eps));
      }
      memmove(&buffer[1], &buffer[0], (S - 1) * sizeof(F));
      buffer[0] = sample;
    }

    void reset() {
      for( int i = 0; i < S + D; i++ )
        weights[i] = eg[i] = buffer[i] = 0.;
    }
};

#endif //PAQ8PX_LMS_HPP
