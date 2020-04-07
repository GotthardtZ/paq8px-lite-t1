#ifndef PAQ8PX_OLS_HPP
#define PAQ8PX_OLS_HPP

#include "Mixer.hpp"
#include "Shared.hpp"
#include <cmath>
#include <cstdint>

/**
 * Ordinary Least Squares predictor
 * @tparam F
 * @tparam T
 * @tparam hasZeroMean
 */
template<typename F, typename T, const bool hasZeroMean = true>
class OLS {
    static constexpr F ftol = 1E-8;
    static constexpr F sub = F(int64_t(!hasZeroMean) << (8 * sizeof(T) - 1));
    Shared *shared = Shared::getInstance();

private:
    int n, kMax, km, index;
    F lambda, nu;
    F *x, *w, *b;
    F **mCovariance, **mCholesky;

    auto factor() -> int {
      // copy the matrix
      for( int i = 0; i < n; i++ ) {
        for( int j = 0; j < n; j++ ) {
          mCholesky[i][j] = mCovariance[i][j];
        }
      }

      for( int i = 0; i < n; i++ ) {
        mCholesky[i][i] += nu;
      }
      for( int i = 0; i < n; i++ ) {
        for( int j = 0; j < i; j++ ) {
          F sum = mCholesky[i][j];
          for( int k = 0; k < j; k++ ) {
            sum -= (mCholesky[i][k] * mCholesky[j][k]);
          }
          mCholesky[i][j] = sum / mCholesky[j][j];
        }
        F sum = mCholesky[i][i];
        for( int k = 0; k < i; k++ ) {
          sum -= (mCholesky[i][k] * mCholesky[i][k]);
        }
        if( sum > ftol ) {
          mCholesky[i][i] = sqrt(sum);
        } else {
          return 1;
        }
      }
      return 0;
    }

    void solve() {
      for( int i = 0; i < n; i++ ) {
        F sum = b[i];
        for( int j = 0; j < i; j++ ) {
          sum -= (mCholesky[i][j] * w[j]);
        }
        w[i] = sum / mCholesky[i][i];
      }
      for( int i = n - 1; i >= 0; i-- ) {
        F sum = w[i];
        for( int j = i + 1; j < n; j++ ) {
          sum -= (mCholesky[j][i] * w[j]);
        }
        w[i] = sum / mCholesky[i][i];
      }
    }

public:
    OLS(int n, int kMax = 1, F lambda = 0.998, F nu = 0.001) : n(n), kMax(kMax), lambda(lambda), nu(nu) {
      km = index = 0;
      x = new F[n], w = new F[n], b = new F[n];
      mCovariance = new F *[n], mCholesky = new F *[n];
      for( int i = 0; i < n; i++ ) {
        x[i] = w[i] = b[i] = 0.;
        mCovariance[i] = new F[n], mCholesky[i] = new F[n];
        for( int j = 0; j < n; j++ ) {
          mCovariance[i][j] = mCholesky[i][j] = 0.;
        }
      }
    }

    ~OLS() {
      delete x, delete w, delete b;
      for( int i = 0; i < n; i++ ) {
        delete mCovariance[i];
        delete mCholesky[i];
      }
      delete[] mCovariance, delete[] mCholesky;
    }

    void add(const T val) {
      assert(index < n);
      x[index++] = F(val) - sub;
    }

    void addFloat(const F val) {
      assert(index < n);
      x[index++] = val - sub;
    }

    auto predict(const T **p) -> F {
      F sum = 0.;
      for( int i = 0; i < n; i++ ) {
        sum += w[i] * (x[i] = F(*p[i]) - sub);
      }
      return sum + sub;
    }

    auto predict() -> F {
      assert(index == n);
      index = 0;
      F sum = 0.;
      for( int i = 0; i < n; i++ ) {
        sum += w[i] * x[i];
      }
      return sum + sub;
    }

    inline void update(const T val) {
#ifdef __GNUC__
      if( shared->chosenSimd == SIMD_AVX2 ) {
        updateAVX2(val);
      } else
#endif
      {
        updateUnrolled(val);
      }
    }

#ifdef __GNUC__

    __attribute__((target("avx2")))
#endif
    void updateAVX2(const T val) {
      F mul = 1.0 - lambda;
      for( int j = 0; j < n; j++ ) {
        for( int i = 0; i < n; i++ ) {
          mCovariance[j][i] = lambda * mCovariance[j][i] + mul * (x[j] * x[i]);
        }
      }
      mul *= (F(val) - sub);
      for( int i = 0; i < n; i++ ) {
        b[i] = lambda * b[i] + mul * x[i];
      }
      km++;
      if( km >= kMax ) {
        if( !factor()) {
          solve();
        }
        km = 0;
      }
    }

    void updateUnrolled(const T val) {
      F mul = 1.0 - lambda;
      int l = n - (n & 3);
      int i = 0;
      for( int j = 0; j < n; j++ ) {
        for( i = 0; i < l; i += 4 ) {
          mCovariance[j][i] = lambda * mCovariance[j][i] + mul * (x[j] * x[i]);
          mCovariance[j][i + 1] = lambda * mCovariance[j][i + 1] + mul * (x[j] * x[i + 1]);
          mCovariance[j][i + 2] = lambda * mCovariance[j][i + 2] + mul * (x[j] * x[i + 2]);
          mCovariance[j][i + 3] = lambda * mCovariance[j][i + 3] + mul * (x[j] * x[i + 3]);
        }
        for( ; i < n; i++ ) {
          mCovariance[j][i] = lambda * mCovariance[j][i] + mul * (x[j] * x[i]);
        }
      }
      mul *= (F(val) - sub);
      for( i = 0; i < l; i += 4 ) {
        b[i] = lambda * b[i] + mul * x[i];
        b[i + 1] = lambda * b[i + 1] + mul * x[i + 1];
        b[i + 2] = lambda * b[i + 2] + mul * x[i + 2];
        b[i + 3] = lambda * b[i + 3] + mul * x[i + 3];
      }
      for( ; i < n; i++ ) {
        b[i] = lambda * b[i] + mul * x[i];
      }
      km++;
      if( km >= kMax ) {
        if( !factor()) {
          solve();
        }
        km = 0;
      }
    }
};

#endif //PAQ8PX_OLS_HPP
