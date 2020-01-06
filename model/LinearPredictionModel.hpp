#ifndef PAQ8PX_LINEARPREDICTIONMODEL_HPP
#define PAQ8PX_LINEARPREDICTIONMODEL_HPP

#include "../OLS.hpp"

class LinearPredictionModel {
private:
    static constexpr int nOLS = 3;
    static constexpr int nSSM = nOLS + 2;

public:
    static constexpr int MIXERINPUTS = nSSM * SmallStationaryContextMap::MIXERINPUTS; // 10
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;

private:
    const Shared *const shared;
    SmallStationaryContextMap sMap[nSSM];
    OLS<double, uint8_t> ols[nOLS] {{32, 4, 0.995},
                                    {32, 4, 0.995},
                                    {32, 4, 0.995}};
    uint8_t prd[nSSM] {0};

public:
    LinearPredictionModel(const Shared *const sh) : shared(sh),
            sMap {/* SmallStationaryContextMap : BitsOfContext, InputBits, Rate, Scale */
                    {sh, 11, 1, 6, 128},
                    {sh, 11, 1, 6, 128},
                    {sh, 11, 1, 6, 128},
                    {sh, 11, 1, 6, 128},
                    {sh, 11, 1, 6, 128}} {}

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        INJECT_SHARED_buf
        const uint8_t W = buf(1), WW = buf(2), WWW = buf(3);
        int i = 0;
        for( ; i < nOLS; i++ )
          ols[i].update(W);
        for( i = 1; i <= 32; i++ ) {
          ols[0].add(buf(i));
          ols[1].add(buf(i * 2 - 1));
          ols[2].add(buf(i * 2));
        }
        for( i = 0; i < nOLS; i++ )
          prd[i] = clip((int) floor(ols[i].predict()));
        prd[i++] = clip(W * 2 - WW);
        prd[i] = clip(W * 3 - WW * 3 + WWW);
      }
      INJECT_SHARED_c0
      const uint8_t B = c0 << (8 - bpos);
      for( int i = 0; i < nSSM; i++ ) {
        sMap[i].set((prd[i] - B) * 8 + bpos);
        sMap[i].mix(m);
      }
    }
};

#endif //PAQ8PX_LINEARPREDICTIONMODEL_HPP
