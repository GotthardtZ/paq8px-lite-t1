#ifndef PAQ8PX_AUDIO8BITMODEL_HPP
#define PAQ8PX_AUDIO8BITMODEL_HPP

#include "AudioModel.hpp"
#include <cstdint>

#ifdef USE_AUDIOMODEL

class Audio8BitModel : AudioModel {
private:
    static constexpr int nOLS = 8;
    static constexpr int nLMS = 3;
    static constexpr int nSSM = nOLS + nLMS + 3;
    static constexpr int nCtx = 3;

public:
    static constexpr int MIXERINPUTS = nCtx * nSSM * SmallStationaryContextMap::MIXERINPUTS;
    static constexpr int MIXERCONTEXTS = 4096 + 2048 + 2048 + 256 + 10; //8458
    static constexpr int MIXERCONTEXTSETS = 5;

private:
    SmallStationaryContextMap sMap1b[nSSM][nCtx];
    OLS<double, int8_t> ols[nOLS][2] {{{128, 24, 0.9975}, {128, 24, 0.9975}},
                                      {{90,  30, 0.9965}, {90,  30, 0.9965}},
                                      {{90,  31, 0.996},  {90,  31, 0.996}},
                                      {{90,  32, 0.995},  {90,  32, 0.995}},
                                      {{90,  33, 0.995},  {90,  33, 0.995}},
                                      {{90,  34, 0.9985}, {90,  34, 0.9985}},
                                      {{28,  4,  0.98},   {28,  4,  0.98}},
                                      {{28,  3,  0.992},  {28,  3,  0.992}}};
    LMS<float, int8_t> lms[nLMS][2] {{{1280, 640, 3e-5f,   2e-5f}, {1280, 640, 3e-5f,   2e-5f}},
                                     {{640,  64,  8e-5f,   1e-5f}, {640,  64,  8e-5f,   1e-5f}},
                                     {{2450, 8,   1.6e-5f, 1e-6f}, {2450, 8,   1.6e-5f, 1e-6f}}};
    int prd[nSSM][2][2] {0}, residuals[nSSM][2] {0};
    int stereo = 0, ch = 0;
    uint32_t mask = 0, errLog = 0, mxCtx = 0;

public:
    Audio8BitModel(const Shared *const sh, ModelStats *st) : AudioModel(sh, st),
            sMap1b {/* SmallStationaryContextMap : BitsOfContext, InputBits, Rate, Scale */
                    /*nOLS: 0-3*/ {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                    /*nOLS: 4-7*/
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 128}, {sh, 11, 1, 9, 128}, {sh, 11, 1, 7, 86}},
                    /*nLMS: 0-2*/
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                    /*nSSM: 0-2*/
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}},
                                  {{sh, 11, 1, 6, 86},  {sh, 11, 1, 9, 86},  {sh, 11, 1, 7, 86}}} {}

    void setParam(int info) {
      INJECT_STATS_blpos
      INJECT_SHARED_bpos
      if( blpos == 0 && bpos == 0 ) {
        assert((info & 2) == 0);
        stereo = (info & 1U);
        mask = 0;
        stats->Wav = stereo + 1;
        wMode = info;
        for( int i = 0; i < nLMS; i++ )
          lms[i][0].reset(), lms[i][1].reset();
      }
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      INJECT_SHARED_c0
      INJECT_SHARED_c1
      if( bpos == 0 ) {
        INJECT_STATS_blpos
        ch = (stereo) ? blpos & 1U : 0;
        const int8_t s = int(((wMode & 4U) > 0) ? c1 ^ 128U : c1) - 128U;
        const int pCh = ch ^stereo;
        int i = 0;
        for( errLog = 0; i < nOLS; i++ ) {
          ols[i][pCh].update(s);
          residuals[i][pCh] = s - prd[i][pCh][0];
          const auto absResidual = (uint32_t) abs(residuals[i][pCh]);
          mask += mask + (absResidual > 4);
          errLog += square(absResidual);
        }
        for( int j = 0; j < nLMS; j++ )
          lms[j][pCh].update(s);
        for( ; i < nSSM; i++ )
          residuals[i][pCh] = s - prd[i][pCh][0];
        errLog = min(0xF, ilog2(errLog));
        stats->Audio = mxCtx = ilog2(min(0x1F, bitCount(mask))) * 2 + ch;

        int k1 = 90, k2 = k1 - 12 * stereo;
        for( int j = (i = 1); j <= k1; j++, i += 1U << ((j > 8) + (j > 16) + (j > 64)))
          ols[1][ch].add(x1(i));
        for( int j = (i = 1); j <= k2; j++, i += 1U << ((j > 5) + (j > 10) + (j > 17) + (j > 26) + (j > 37)))
          ols[2][ch].add(x1(i));
        for( int j = (i = 1); j <= k2; j++, i += 1U << ((j > 3) + (j > 7) + (j > 14) + (j > 20) + (j > 33) + (j > 49)))
          ols[3][ch].add(x1(i));
        for( int j = (i = 1); j <= k2; j++, i += 1 + (j > 4) + (j > 8))
          ols[4][ch].add(x1(i));
        for( int j = (i = 1); j <= k1; j++, i += 2 + ((j > 3) + (j > 9) + (j > 19) + (j > 36) + (j > 61)))
          ols[5][ch].add(x1(i));
        if( stereo ) {
          for( i = 1; i <= k1 - k2; i++ ) {
            const auto s = (double) x2(i);
            ols[2][ch].addFloat(s);
            ols[3][ch].addFloat(s);
            ols[4][ch].addFloat(s);
          }
        }
        k1 = 28, k2 = k1 - 6 * stereo;
        for( i = 1; i <= k2; i++ ) {
          const auto s = (double) x1(i);
          ols[0][ch].addFloat(s);
          ols[6][ch].addFloat(s);
          ols[7][ch].addFloat(s);
        }
        for( ; i <= 96; i++ )
          ols[0][ch].add(x1(i));
        if( stereo ) {
          for( i = 1; i <= k1 - k2; i++ ) {
            const auto s = (double) x2(i);
            ols[0][ch].addFloat(s);
            ols[6][ch].addFloat(s);
            ols[7][ch].addFloat(s);
          }
          for( ; i <= 32; i++ )
            ols[0][ch].add(x2(i));
        } else
          for( ; i <= 128; i++ )
            ols[0][ch].add(x1(i));

        for( i = 0; i < nOLS; i++ )
          prd[i][ch][0] = signedClip8((int) floor(ols[i][ch].predict()));
        for( ; i < nOLS + nLMS; i++ )
          prd[i][ch][0] = signedClip8((int) floor(lms[i - nOLS][ch].predict(s)));
        prd[i++][ch][0] = signedClip8(x1(1) * 2 - x1(2));
        prd[i++][ch][0] = signedClip8(x1(1) * 3 - x1(2) * 3 + x1(3));
        prd[i][ch][0] = signedClip8(x1(1) * 4 - x1(2) * 6 + x1(3) * 4 - x1(4));
        for( i = 0; i < nSSM; i++ )
          prd[i][ch][1] = signedClip8(prd[i][ch][0] + residuals[i][pCh]);
      }
      const int8_t B = c0 << (8U - bpos);
      for( int i = 0; i < nSSM; i++ ) {
        const uint32_t ctx = (prd[i][ch][0] - B) * 8 + bpos;
        sMap1b[i][0].set(ctx);
        sMap1b[i][1].set(ctx);
        sMap1b[i][2].set((prd[i][ch][1] - B) * 8 + bpos);
        sMap1b[i][0].mix(m);
        sMap1b[i][1].mix(m);
        sMap1b[i][2].mix(m);
      }
      m.set((errLog << 8U) | c0, 4096);
      m.set((uint8_t(mask) << 3U) | (ch << 2) | (bpos >> 1), 2048);
      m.set((mxCtx << 7U) | (c1 >> 1), 1280);
      m.set((errLog << 4U) | (ch << 3) | bpos, 256);
      m.set(mxCtx, 10);
    }
};

#endif //USE_AUDIOMODEL
#endif //PAQ8PX_AUDIO8BITMODEL_HPP
