#ifndef PAQ8PX_MIXER_HPP
#define PAQ8PX_MIXER_HPP

#include "utils.hpp"

#ifdef __GNUC__

__attribute__((target("avx2")))
#endif
static int dotProductSimdAvx2(const short *const t, const short *const w, int n) {
  __m256i sum = _mm256_setzero_si256();

  while((n -= 16) >= 0 ) {
    __m256i tmp = _mm256_madd_epi16(*(__m256i *) &t[n], *(__m256i *) &w[n]);
    tmp = _mm256_srai_epi32(tmp, 8);
    sum = _mm256_add_epi32(sum, tmp);
  }

  __m128i lo = _mm256_extractf128_si256(sum, 0);
  __m128i hi = _mm256_extractf128_si256(sum, 1);

  __m128i newSum = _mm_hadd_epi32(lo, hi);
  newSum = _mm_add_epi32(newSum, _mm_srli_si128(newSum, 8));
  newSum = _mm_add_epi32(newSum, _mm_srli_si128(newSum, 4));
  return _mm_cvtsi128_si32(newSum);
}

#ifdef __GNUC__

__attribute__((target("avx2")))
#endif
static void trainSimdAvx2(const short *const t, short *const w, int n, const int e) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i err = _mm256_set1_epi16(short(e));

  while((n -= 16) >= 0 ) {
    __m256i tmp = _mm256_adds_epi16(*(__m256i *) &t[n], *(__m256i *) &t[n]);
    tmp = _mm256_mulhi_epi16(tmp, err);
    tmp = _mm256_adds_epi16(tmp, one);
    tmp = _mm256_srai_epi16(tmp, 1);
    tmp = _mm256_adds_epi16(tmp, *(__m256i *) &w[n]);
    *(__m256i *) &w[n] = tmp;
  }
}

#ifdef __GNUC__

__attribute__((target("sse2")))
#endif
static int dotProductSimdSse2(const short *const t, const short *const w, int n) {
  __m128i sum = _mm_setzero_si128();

  while((n -= 8) >= 0 ) {
    __m128i tmp = _mm_madd_epi16(*(__m128i *) &t[n], *(__m128i *) &w[n]);
    tmp = _mm_srai_epi32(tmp, 8);
    sum = _mm_add_epi32(sum, tmp);
  }

  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
  return _mm_cvtsi128_si32(sum);
}

#ifdef __GNUC__

__attribute__((target("sse2")))
#endif
static void trainSimdSse2(const short *const t, short *const w, int n, const int e) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i err = _mm_set1_epi16(short(e));

  while((n -= 8) >= 0 ) {
    __m128i tmp = _mm_adds_epi16(*(__m128i *) &t[n], *(__m128i *) &t[n]);
    tmp = _mm_mulhi_epi16(tmp, err);
    tmp = _mm_adds_epi16(tmp, one);
    tmp = _mm_srai_epi16(tmp, 1);
    tmp = _mm_adds_epi16(tmp, *(__m128i *) &w[n]);
    *(__m128i *) &w[n] = tmp;
  }
}

static int dotProductSimdNone(const short *const t, const short *const w, int n) {
  int sum = 0;
  while((n -= 2) >= 0 ) {
    sum += (t[n] * w[n] + t[n + 1] * w[n + 1]) >> 8;
  }
  return sum;
}

static void trainSimdNone(const short *const t, short *const w, int n, const int err) {
  while((n -= 1) >= 0 ) {
    int wt = w[n] + ((((t[n] * err * 2) >> 16) + 1) >> 1);
    if( wt < -32768 )
      wt = -32768;
    else if( wt > 32767 )
      wt = 32767;
    w[n] = (short) wt;
  }
}

typedef enum {
    SIMD_NONE, SIMD_SSE2, SIMD_AVX2
} SIMD;

class Mixer : protected IPredictor {
protected:
    const Shared *const shared;
    const uint32_t n, m, s; // max inputs, max contexts, max context sets
    int scaleFactor; // scale factor for dot product
    Array<short, 32> tx; // n inputs from add()
    Array<short, 32> wx; // n*m weights
    Array<uint32_t> cxt; // s contexts
    Array<ErrorInfo> info; // stats for the adaptive learning rates
    Array<int> rates; // learning rates
    uint32_t numContexts {}; // number of contexts (0 to s)
    uint32_t base {}; // offset of next context
    uint32_t nx {}; // number of inputs in tx, 0 to n
    Array<int> pr; // last result (scaled 12 bits)
public:
    /**
     * Mixer m(n, m, s=1, w=0) combines models using m neural networks with
     * n inputs each, of which up to s may be selected.  If s > 1 then
     * the outputs of these neural networks are combined using another
     * neural network (with arguments s, 1, 1).  If s = 1 then the
     * output is direct.  The weights are initially w (+-32K).
     * @param sh
     * @param n
     * @param m
     * @param s
     */
    Mixer(const Shared *const sh, const int n, const int m, const int s) : shared(sh), n(n), m(m), s(s), scaleFactor(0), tx(n), wx(n * m),
            cxt(s), info(s), rates(s), pr(s) {
      for( uint64_t i = 0; i < s; ++i ) {
        pr[i] = 2048; //initial p=0.5
        rates[i] = DEFAULT_LEARNING_RATE;
        info[i].reset();
      }
      reset();
    }

    ~Mixer() override = default;
    /**
     * // m.p() returns the output prediction that the next bit is 1 as a 12 bit number (0 to 4095).
     * @return
     */
    virtual int p() = 0;
    virtual void setScaleFactor(int sf0, int sf1) = 0;

    /**
     * Input x (call up to n times)
     * m.add(stretch(p)) inputs a prediction from one of \n models.  The
     * prediction should be positive to predict a 1 bit, negative for 0,
     * nominally +-256 to +-2K.  The maximum allowed value is +-32K but
     * using such large values may cause overflow if \n is large.
     * @param x
     */

    void add(const int x) {
      assert(nx < n);
      assert(x == short(x));
      tx[nx++] = (short) x;
    }

    /**
     *  m.set(cx, range) selects cx as one of \range neural networks to
     *  use.  0 <= cx < range. Should be called up to s times such
     *  that the total of the ranges is <= m.
     * @param cx
     * @param range
     * @param rate
     */
    void set(const uint32_t cx, const uint32_t range, const int rate = DEFAULT_LEARNING_RATE) {
      assert(numContexts < s);
      assert(cx < range);
      assert(base + range <= m);
      if( !(options & OPTION_ADAPTIVE))
        rates[numContexts] = rate;
      cxt[numContexts++] = base + cx;
      base += range;
      //printf("numContexts: %d base: %d\n",numContexts,range); //for debugging: how many input sets do we have?
    }

    void reset() {
      nx = base = numContexts = 0;
    }
};

// for training NormalModel, WordModel and ExeModel
class DummyMixer : public Mixer {
public:
    DummyMixer(const Shared *const sh, const int n, const int m, const int s) : Mixer(sh, n, m, s) {}

    void update() override { reset(); }

    int p() override {
      updater.subscribe(this);
      return 2048;
    }

    void setScaleFactor(const int, const int) override {};
};

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
      assert(n > 0 && n > 0 && (n & (simdWidth() - 1)) == 0 && m > 0 && s >= 1);
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

    // predict next bit
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

static SIMD chosenSimd = SIMD_NONE; //default value, will be overridden by the CPU dispatcher, and may be overridden from the command line
class MixerFactory {
public:
    static void setSimd(SIMD simd) {
      chosenSimd = simd;
    }

    static Mixer *createMixer(const Shared *const sh, const int n, const int m, const int s) {
      if( chosenSimd == SIMD_NONE )
        return new SIMDMixer<SIMD_NONE>(sh, n, m, s);
      if( chosenSimd == SIMD_SSE2 )
        return new SIMDMixer<SIMD_SSE2>(sh, n, m, s);
      if( chosenSimd == SIMD_AVX2 )
        return new SIMDMixer<SIMD_AVX2>(sh, n, m, s);
      assert(false);
      return nullptr;
    }
};

#endif //PAQ8PX_MIXER_HPP
