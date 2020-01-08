#ifndef PAQ8PX_SIMDMIXER_HPP
#define PAQ8PX_SIMDMIXER_HPP

template<SIMD simd>
class SIMDMixer : public Mixer {
private:
    //define padding requirements
    [[nodiscard]] constexpr inline int simdWidth() const {
      if( simd == SIMD_AVX2 )
        return 32 / sizeof(short); //256 bit (32 byte) data size
      if( simd == SIMD_SSE2 )
        return 16 / sizeof(short); //128 bit (16 byte) data size
      if( simd == SIMD_NONE )
        return 4 / sizeof(short); //processes 2 shorts at once -> width is 4 bytes
      assert(false);
    }

    SIMDMixer *mp; // points to a Mixer to combine results
public:
    SIMDMixer(const Shared *sh, const int n, const int m, const int s) : Mixer(sh, ((n + (simdWidth() - 1)) & -(simdWidth())), m, s) {
      assert(n > 0);
      // TODO: This assertion fails
//      assert((n & simdWidth() - 1) == 0);
      assert(m > 0);
      assert(s >= 1);
      mp = (s > 1) ? new SIMDMixer<simd>(sh, s, 1, 1) : nullptr;
    }

    ~SIMDMixer() override {
      delete mp;
    }

    void setScaleFactor(const int sf0, const int sf1) override {
      scaleFactor = sf0;
      if( mp ) {
        mp->setScaleFactor(sf1, 0);
      }
    }

    /**
     * Adjust weights to minimize coding cost of last prediction
     * m.update() trains the network where the expected output is the last bit (in the global variable y).
     */
    void update() override {
      INJECT_SHARED_y
      const int target = y << 12U;
      if( nx > 0 )
        for( uint64_t i = 0; i < numContexts; ++i ) {
          const int err = target - pr[i];
          if( simd == SIMD_NONE )
            trainSimdNone(&tx[0], &wx[cxt[i] * n], nx, err * rates[i]);
          if( simd == SIMD_SSE2 )
            trainSimdSse2(&tx[0], &wx[cxt[i] * n], nx, err * rates[i]);
          if( simd == SIMD_AVX2 )
            trainSimdAvx2(&tx[0], &wx[cxt[i] * n], nx, err * rates[i]);
          if( options & OPTION_ADAPTIVE ) {
            const uint32_t logErr = min(0xF, ilog2(abs(err)));
            info[i].sum -= square(info[i].data[1] >> 28U);
            info[i].data[1] <<= 4U;
            info[i].data[1] |= info[i].data[0] >> 28U;
            info[i].data[0] <<= 4U;
            info[i].data[0] |= logErr;
            info[i].sum += square(logErr);
            info[i].collected += info[i].collected < 4096;
            info[i].mask <<= 1U;
            info[i].mask |= (logErr <= ((info[i].data[0] >> 4U) & 0xFU));
            const uint32_t count = bitCount(info[i].mask);
            if( info[i].collected >= 64 && (info[i].sum > 1500 + uint32_t(rates[i]) * 64 || count < 9 || (info[i].mask & 0xFFU) == 0)) {
              rates[i] = DEFAULT_LEARNING_RATE;
              memset(&info[i], 0, sizeof(ErrorInfo));
            } else if( info[i].collected == 4096 && info[i].sum >= 56 && info[i].sum <= 144 && count > 28 - uint32_t(rates[i]) &&
                       ((info[i].mask & 0xFFU) == 0xFFU)) {
              rates[i] -= rates[i] > 2;
              info[i].reset();
            }
          }
        }
      reset();
    }

    /**
     * Predict next bit
     * @return
     */
    int p() override {
      updater.subscribe(this);
      assert(scaleFactor > 0);
      //if(mp)printf("nx: %d, numContexts: %d, base: %d\n",nx, numContexts, base); //for debugging: how many inputs do we have?
      while( nx & (simdWidth() - 1))
        tx[nx++] = 0; // pad
      if( mp ) { // combine outputs
        for( uint64_t i = 0; i < numContexts; ++i ) {
          int dp = 0;
          if( simd == SIMD_NONE )
            dp = dotProductSimdNone(&tx[0], &wx[cxt[i] * n], nx);
          if( simd == SIMD_SSE2 )
            dp = dotProductSimdSse2(&tx[0], &wx[cxt[i] * n], nx);
          if( simd == SIMD_AVX2 )
            dp = dotProductSimdAvx2(&tx[0], &wx[cxt[i] * n], nx);
          dp = (dp * scaleFactor) >> 16U;
          if( dp < -2047 )
            dp = -2047;
          else if( dp > 2047 )
            dp = 2047;
          mp->add(dp);
          pr[i] = squash(dp);
        }
        mp->set(0, 1);
        return mp->p();
      } else { // s=1 context
        int dp = 0;
        if( simd == SIMD_NONE )
          dp = dotProductSimdNone(&tx[0], &wx[cxt[0] * n], nx);
        if( simd == SIMD_SSE2 )
          dp = dotProductSimdSse2(&tx[0], &wx[cxt[0] * n], nx);
        if( simd == SIMD_AVX2 )
          dp = dotProductSimdAvx2(&tx[0], &wx[cxt[0] * n], nx);
        dp = (dp * scaleFactor) >> 16U;
        return pr[0] = squash(dp);
      }
    }
};

#endif //PAQ8PX_SIMDMIXER_HPP
