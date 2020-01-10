#ifndef PAQ8PX_MIXER_HPP
#define PAQ8PX_MIXER_HPP

#include <immintrin.h>
#include "utils.hpp"
#include "IPredictor.hpp"

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

class Mixer : protected IPredictor {
protected:
    Shared *shared = Shared::getInstance();
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
     * @param n
     * @param m
     * @param s
     */
    Mixer(const int n, const int m, const int s) : n(n), m(m), s(s), scaleFactor(0), tx(n), wx(n * m),
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
      if( !(shared->options & OPTION_ADAPTIVE))
        rates[numContexts] = rate;
      cxt[numContexts++] = base + cx;
      base += range;
      //printf("numContexts: %d base: %d\n",numContexts,range); //for debugging: how many input sets do we have?
    }

    void reset() {
      nx = base = numContexts = 0;
    }
};

static SIMD chosenSimd = SIMD_NONE; //default value, will be overridden by the CPU dispatcher, and may be overridden from the command line
#include "MixerFactory.hpp"

#endif //PAQ8PX_MIXER_HPP
