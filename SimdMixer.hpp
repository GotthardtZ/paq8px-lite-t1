#ifndef PAQ8PX_SIMDMIXER_HPP
#define PAQ8PX_SIMDMIXER_HPP

#include "Mixer.hpp"
#include "Squash.hpp"

template<SIMDType simd>
class SIMDMixer : public Mixer {
private:

    /**
     * Define SIMD padding requirements.
     */
    [[nodiscard]] constexpr inline auto simdWidth() const -> int {
      if( simd == SIMDType::SIMD_AVX2 ) {
        return 32 / sizeof(short); // 256 bit (32 byte) data size
      }
      else if( simd == SIMDType::SIMD_SSE2 || simd == SIMDType::SIMD_SSSE3 || simd == SIMDType::SIMD_NEON ) {
        return 16 / sizeof(short); // 128 bit (16 byte) data size
      }
      else if( simd == SIMDType::SIMD_NONE ) {
        return 4 / sizeof(short); // Processes 2 shorts at once -> width is 4 bytes
      }
      assert(false);
    }

public:
  SIMDMixer* mp; /**< points to a Mixer to combine results */
  
  SIMDMixer(const Shared* const sh, const int n, const int m, const int s) :
      Mixer(sh, ((n + (simdWidth() - 1)) & -(simdWidth())), m, s) {
      assert((this->n & (simdWidth() - 1)) == 0);
      assert(this->m > 0);
      assert(this->s > 0);
      mp = (s > 1) ? new SIMDMixer<simd>(sh, s, 1, 1) : nullptr;
    }

    ~SIMDMixer() {
      delete mp;
    }

    void setScaleFactor(const int sf0, const int sf1) override {
      scaleFactor = sf0;
      if( mp ) {
        mp->setScaleFactor(sf1, 0);
      }
    }

    /**
     * Adjust weights to minimize coding cost of last prediction.
     * Trains the network where the expected output is the last bit (in the shared variable y).
     */
    void update() override {
      if (mp)
        mp->update();
      INJECT_SHARED_y
      const int target = y << 12;
      for( uint64_t i = 0; i < numContexts; ++i ) {
        int err = target - pr[i];
        int rate = rates[i];
        if (mp == nullptr) {
          if (rate > MIN_LEARNING_RATE_S1) rate--;
        }
        else {
          if (rate > MIN_LEARNING_RATE_SN) rate--;
        }
        rates[i] = rate;
        if (simd == SIMDType::SIMD_NONE) {
          trainSimdNone(&tx[0], &wx[cxt[i] * n], n, (err * rate) >> 16);
        }
        else if (simd == SIMDType::SIMD_SSE2 || simd == SIMDType::SIMD_SSSE3) {
          trainSimdSse2(&tx[0], &wx[cxt[i] * n], n, (err * rate) >> 16);
        }
        else if (simd == SIMDType::SIMD_AVX2) {
          trainSimdAvx2(&tx[0], &wx[cxt[i] * n], n, (err * rate) >> 16);
        }
        else if (simd == SIMDType::SIMD_NEON) {
          trainSimdNeon(&tx[0], &wx[cxt[i] * n], n, (err * rate) >> 16);
        }
      }
      reset();
    }

    /**
     * Predict next bit
     * @return prediction
     */
    auto p() -> int override {
      //shared->GetUpdateBroadcaster()->subscribe(this);
      assert(scaleFactor > 0);
      //if(mp)printf("nx: %d, numContexts: %d, base: %d\n",nx, numContexts, base); //for debugging: how many inputs do we have?
      if( mp ) { // combine outputs
        for( uint64_t i = 0; i < numContexts; ++i ) {
          int dp = 0;
          if (simd == SIMDType::SIMD_NONE) {
            dp = dotProductSimdNone(&tx[0], &wx[cxt[i] * n], n);
          }
          else if (simd == SIMDType::SIMD_SSE2 || simd == SIMDType::SIMD_SSSE3) {
            dp = dotProductSimdSse2(&tx[0], &wx[cxt[i] * n], n);
          }
          else if (simd == SIMDType::SIMD_AVX2) {
            dp = dotProductSimdAvx2(&tx[0], &wx[cxt[i] * n], n);
          }
          else if (simd == SIMDType::SIMD_NEON) {
            dp = dotProductSimdNeon(&tx[0], &wx[cxt[i] * n], n);
          }
          else {
            static_assert("Unknown SIMD parameter");
          }
          dp = (dp * scaleFactor) >> 16;
          if (dp < -2047) {
            dp = -2047;
          }
          else if (dp > 2047) {
            dp = 2047;
          }
          mp->add(dp);
          pr[i] = squash(dp);
        }
        mp->set(0, 1);
        return mp->p();
      } // s=1 context
      int dp;
      if( simd == SIMDType::SIMD_NONE ) {
        dp = dotProductSimdNone(&tx[0], &wx[cxt[0] * n], n);
      }
      else if( simd == SIMDType::SIMD_SSE2 || simd == SIMDType::SIMD_SSSE3 ) {
        dp = dotProductSimdSse2(&tx[0], &wx[cxt[0] * n], n);
      }
      else if( simd == SIMDType::SIMD_AVX2 ) {
        dp = dotProductSimdAvx2(&tx[0], &wx[cxt[0] * n], n);
      }
      else if (simd == SIMDType::SIMD_NEON) {
        dp = dotProductSimdNeon(&tx[0], &wx[cxt[0] * n], n);
      }
      else {
        static_assert("Unknown SIMD parameter");
      }
      dp = (dp * scaleFactor) >> 16;
      return pr[0] = squash(dp);
    }
};

#endif //PAQ8PX_SIMDMIXER_HPP