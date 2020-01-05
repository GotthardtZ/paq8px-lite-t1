#ifndef PAQ8PX_IMAGE24BITMODEL_HPP
#define PAQ8PX_IMAGE24BITMODEL_HPP

//////////////////////////// Image24bitModel /////////////////////////////////

inline uint8_t paeth(uint8_t const W, uint8_t const N, uint8_t const NW) {
  int p = W + N - NW;
  int pW = abs(p - (int) W), pN = abs(p - (int) N), pNW = abs(p - (int) NW);
  if( pW <= pN && pW <= pNW )
    return W;
  else if( pN <= pNW )
    return N;
  return NW;
}

// Model for filtered (PNG) or unfiltered 24/32-bit image data

class Image24bitModel {
private:
    static constexpr int nSM0 = 18;
    static constexpr int nSM1 = 76;
    static constexpr int nOLS = 6;
    static constexpr int nSM = nSM0 + nSM1 + nOLS;
    static constexpr int nSSM = 59;
    static constexpr int nCM = 45;

public:
    static constexpr int MIXERINPUTS =
            nSSM * SmallStationaryContextMap::MIXERINPUTS + nSM * StationaryMap::MIXERINPUTS +
            nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS);
    static constexpr int MIXERCONTEXTS =
            6 + 256 + 512 + 2048 + 8 * 32 + 6 * 64 + 256 * 2 + 1024 + 8192 + 8192 + 8192 + 8192 + 256; //38022
    static constexpr int MIXERCONTEXTSETS = 13;

    const Shared *const shared;
    ModelStats *stats;
    ContextMap2 cm;
    SmallStationaryContextMap SCMap[nSSM];
    StationaryMap map[nSM];
    Buf buffer {0x100000}; // internal rotating buffer for (PNG unfiltered) pixel data
    //pixel neighborhood
    uint8_t WWWWWW = 0, WWWWW = 0, WWWW = 0, WWW = 0, WW = 0, W = 0;
    uint8_t NWWWW = 0, NWWW = 0, NWW = 0, NW = 0, N = 0, NE = 0, NEE = 0, NEEE = 0, NEEEE = 0;
    uint8_t NNWWW = 0, NNWW = 0, NNW = 0, NN = 0, NNE = 0, NNEE = 0, NNEEE = 0;
    uint8_t NNNWW = 0, NNNW = 0, NNN = 0, NNNE = 0, NNNEE = 0;
    uint8_t NNNNW = 0, NNNN = 0, NNNNE = 0;
    uint8_t NNNNN = 0;
    uint8_t NNNNNN = 0;
    uint8_t WWp1 = 0, Wp1 = 0, p1 = 0, NWp1 = 0, Np1 = 0, NEp1 = 0, NNp1 = 0;
    uint8_t WWp2 = 0, Wp2 = 0, p2 = 0, NWp2 = 0, Np2 = 0, NEp2 = 0, NNp2 = 0;
    uint8_t px = 0; // current PNG filter prediction
    int info = 0;
    uint32_t alpha = 0, isPNG = 0;
    int color = -1;
    int stride = 3;
    int col = 0;
    int ctx[2] {}, padding = 0, filter = 0, x = 0, w = 0, line = 0, R1 = 0, R2 = 0;
    uint32_t lastPos = 0, lastWasPNG = 0;
    bool filterOn = false;
    int columns[2] = {1, 1}, column[2] {};
    uint8_t mapCtxs[nSM1] = {0}, scMapCtxs[nSSM] = {0}, pOLS[nOLS] = {0};
    static constexpr double lambda[nOLS] = {0.98, 0.87, 0.9, 0.8, 0.9, 0.7};
    static constexpr int num[nOLS] = {32, 12, 15, 10, 14, 8};
    OLS<double, uint8_t> ols[nOLS][4] = {{{num[0], 1, lambda[0]}, {num[0], 1, lambda[0]}, {num[0], 1, lambda[0]}, {num[0], 1, lambda[0]}},
                                         {{num[1], 1, lambda[1]}, {num[1], 1, lambda[1]}, {num[1], 1, lambda[1]}, {num[1], 1, lambda[1]}},
                                         {{num[2], 1, lambda[2]}, {num[2], 1, lambda[2]}, {num[2], 1, lambda[2]}, {num[2], 1, lambda[2]}},
                                         {{num[3], 1, lambda[3]}, {num[3], 1, lambda[3]}, {num[3], 1, lambda[3]}, {num[3], 1, lambda[3]}},
                                         {{num[4], 1, lambda[4]}, {num[4], 1, lambda[4]}, {num[4], 1, lambda[4]}, {num[4], 1, lambda[4]}},
                                         {{num[5], 1, lambda[5]}, {num[5], 1, lambda[5]}, {num[5], 1, lambda[5]}, {num[5], 1, lambda[5]}}};
    const uint8_t *ols_ctx1[32] = {&WWWWWW, &WWWWW, &WWWW, &WWW, &WW, &W, &NWWWW, &NWWW, &NWW, &NW, &N, &NE, &NEE,
                                   &NEEE, &NEEEE, &NNWWW, &NNWW, &NNW, &NN, &NNE, &NNEE, &NNEEE, &NNNWW, &NNNW, &NNN,
                                   &NNNE, &NNNEE, &NNNNW, &NNNN, &NNNNE, &NNNNN, &NNNNNN};
    const uint8_t *ols_ctx2[12] = {&WWW, &WW, &W, &NWW, &NW, &N, &NE, &NEE, &NNW, &NN, &NNE, &NNN};
    const uint8_t *ols_ctx3[15] = {&N, &NE, &NEE, &NEEE, &NEEEE, &NN, &NNE, &NNEE, &NNEEE, &NNN, &NNNE, &NNNEE, &NNNN,
                                   &NNNNE, &NNNNN};
    const uint8_t *ols_ctx4[10] = {&N, &NE, &NEE, &NEEE, &NN, &NNE, &NNEE, &NNN, &NNNE, &NNNN};
    const uint8_t *ols_ctx5[14] = {&WWWW, &WWW, &WW, &W, &NWWW, &NWW, &NW, &N, &NNWW, &NNW, &NN, &NNNW, &NNN, &NNNN};
    const uint8_t *ols_ctx6[8] = {&WWW, &WW, &W, &NNN, &NN, &N, &p1, &p2};
    const uint8_t **ols_ctxs[nOLS] = {&ols_ctx1[0], &ols_ctx2[0], &ols_ctx3[0], &ols_ctx4[0], &ols_ctx5[0],
                                      &ols_ctx6[0]};

public:
    Image24bitModel(const Shared *const sh, ModelStats *st, const uint64_t size) : shared(sh), stats(st),
                                                                                   cm(sh, size, nCM, 64,
                                                                                      CM_USE_RUN_STATS),
                                                                                   SCMap {/* SmallStationaryContextMap : BitsOfContext, InputBits, Rate, Scale */
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 11, 1, 9, 86},
                                                                                           {sh, 0,  8, 9, 86}},
                                                                                   map {/* StationaryMap: BitsOfContext, InputBits, Scale, Limit  */
                                                                                           /*nSM0: 0- 8*/ {sh, 8,  8, 86, 1023},
                                                                                                          {sh, 8,  8, 86, 1023},
                                                                                                          {sh, 8,  8, 86, 1023},
                                                                                                          {sh, 2,  8, 86, 1023},
                                                                                                          {sh, 0,  8, 86, 1023},
                                                                                                          {sh, 15, 1, 86, 1023},
                                                                                                          {sh, 15, 1, 86, 1023},
                                                                                                          {sh, 15, 1, 86, 1023},
                                                                                                          {sh, 15, 1, 86, 1023},
                                                                                           /*nSM0: 9-17*/
                                                                                                          {sh, 15, 1, 86, 1023},
                                                                                                          {sh, 17, 1, 86, 1023},
                                                                                                          {sh, 17, 1, 86, 1023},
                                                                                                          {sh, 17, 1, 86, 1023},
                                                                                                          {sh, 17, 1, 86, 1023},
                                                                                                          {sh, 13, 1, 86, 1023},
                                                                                                          {sh, 13, 1, 86, 1023},
                                                                                                          {sh, 13, 1, 86, 1023},
                                                                                                          {sh, 13, 1, 86, 1023},
                                                                                           /*nSM1: 0- 8*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1: 9-17*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:18-26*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:27-35*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:36-44*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:45-53*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:54-62*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:63-71*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nSM1:72-75*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                           /*nOLS:   0- 5*/
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023},
                                                                                                          {sh, 11, 1, 86, 1023}} {}

    void update() {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        INJECT_SHARED_c1
        INJECT_SHARED_buf
        if( x == 1 && isPNG )
          filter = c1;
        else {
          if( x + padding < w ) {
            color++;
            if( color >= stride )
              color = 0;
          } else {
            if( padding > 0 )
              color = stride;
            else
              color = 0;
          }
          if( isPNG ) {
            switch( filter ) {
              case 1: {
                buffer.add((uint8_t) (c1 + buffer(stride) * (x > stride + 1 || !x)));
                filterOn = x > stride;
                px = buffer(stride);
                break;
              }
              case 2: {
                buffer.add((uint8_t) (c1 + buffer(w) * (filterOn = (line > 0))));
                px = buffer(w);
                break;
              }
              case 3: {
                buffer.add((uint8_t) (c1 + (buffer(w) * (line > 0) + buffer(stride) * (x > stride + 1 || !x)) / 2));
                filterOn = (x > stride || line > 0);
                px = (buffer(stride) * (x > stride) + buffer(w) * (line > 0)) / 2;
                break;
              }
              case 4: {
                buffer.add((uint8_t) (c1 + paeth(buffer(stride) * (x > stride + 1 || !x), buffer(w) * (line > 0),
                                                 buffer(w + stride) * (line > 0 && (x > stride + 1 || !x)))));
                filterOn = (x > stride || line > 0);
                px = paeth(buffer(stride) * (x > stride), buffer(w) * (line > 0),
                           buffer(w + stride) * (x > stride && line > 0));
                break;
              }
              default:
                buffer.add(c1);
                filterOn = false;
                px = 0;
            }
            if( !filterOn )
              px = 0;
          } else
            buffer.add(c1);
        }

        if( x > 0 || !isPNG ) {
          column[0] = (x - isPNG) / columns[0];
          column[1] = (x - isPNG) / columns[1];
          WWWWWW = buffer(6 * stride), WWWWW = buffer(5 * stride), WWWW = buffer(4 * stride), WWW = buffer(
                  3 * stride), WW = buffer(2 * stride), W = buffer(stride);
          NWWWW = buffer(w + 4 * stride), NWWW = buffer(w + 3 * stride), NWW = buffer(w + 2 * stride), NW = buffer(
                  w + stride), N = buffer(w), NE = buffer(w - stride), NEE = buffer(w - 2 * stride), NEEE = buffer(
                  w - 3 * stride), NEEEE = buffer(w - 4 * stride);
          NNWWW = buffer(w * 2 + stride * 3), NNWW = buffer((w + stride) * 2), NNW = buffer(
                  w * 2 + stride), NN = buffer(w * 2), NNE = buffer(w * 2 - stride), NNEE = buffer(
                  (w - stride) * 2), NNEEE = buffer(w * 2 - stride * 3);
          NNNWW = buffer(w * 3 + stride * 2), NNNW = buffer(w * 3 + stride), NNN = buffer(w * 3), NNNE = buffer(
                  w * 3 - stride), NNNEE = buffer(w * 3 - stride * 2);
          NNNNW = buffer(w * 4 + stride), NNNN = buffer(w * 4), NNNNE = buffer(w * 4 - stride);
          NNNNN = buffer(w * 5);
          NNNNNN = buffer(w * 6);
          WWp1 = buffer(stride * 2 + 1), Wp1 = buffer(stride + 1), p1 = buffer(1), NWp1 = buffer(
                  w + stride + 1), Np1 = buffer(w + 1), NEp1 = buffer(w - stride + 1), NNp1 = buffer(w * 2 + 1);
          WWp2 = buffer(stride * 2 + 2), Wp2 = buffer(stride + 2), p2 = buffer(2), NWp2 = buffer(
                  w + stride + 2), Np2 = buffer(w + 2), NEp2 = buffer(w - stride + 2), NNp2 = buffer(w * 2 + 2);

          int j = -1;
          mapCtxs[++j] = clamp4(N + p1 - Np1, W, NW, N, NE);
          mapCtxs[++j] = clamp4(N + p2 - Np2, W, NW, N, NE);
          mapCtxs[++j] = (W + clamp4(NE * 3 - NNE * 3 + NNNE, W, N, NE, NEE)) / 2;
          mapCtxs[++j] = clamp4((W + clip(NE * 2 - NNE)) / 2, W, NW, N, NE);
          mapCtxs[++j] = (W + NEE) / 2;
          mapCtxs[++j] = clip((WWW - 4 * WW + 6 * W + clip(NE * 4 - NNE * 6 + NNNE * 4 - NNNNE)) / 4);
          mapCtxs[++j] = clip(
                  (-WWWW + 5 * WWW - 10 * WW + 10 * W + clamp4(NE * 4 - NNE * 6 + NNNE * 4 - NNNNE, N, NE, NEE, NEEE)) /
                  5);
          mapCtxs[++j] = clip((-4 * WW + 15 * W + 10 * clip(NE * 3 - NNE * 3 + NNNE) -
                               clip(NEEE * 3 - NNEEE * 3 + buffer(w * 3 - 3 * stride))) / 20);
          mapCtxs[++j] = clip((-3 * WW + 8 * W + clamp4(NEE * 3 - NNEE * 3 + NNNEE, NE, NEE, NEEE, NEEEE)) / 6);
          mapCtxs[++j] = clip(
                  (W + clip(NE * 2 - NNE)) / 2 + p1 - (Wp1 + clip(NEp1 * 2 - buffer(w * 2 - stride + 1))) / 2);
          mapCtxs[++j] = clip(
                  (W + clip(NE * 2 - NNE)) / 2 + p2 - (Wp2 + clip(NEp2 * 2 - buffer(w * 2 - stride + 2))) / 2);
          mapCtxs[++j] = clip((-3 * WW + 8 * W + clip(NEE * 2 - NNEE)) / 6 + p1 - (-3 * WWp1 + 8 * Wp1 +
                                                                                   clip(buffer(w - stride * 2 + 1) * 2 -
                                                                                        buffer(w * 2 - stride * 2 +
                                                                                               1))) / 6);
          mapCtxs[++j] = clip((-3 * WW + 8 * W + clip(NEE * 2 - NNEE)) / 6 + p2 - (-3 * WWp2 + 8 * Wp2 +
                                                                                   clip(buffer(w - stride * 2 + 2) * 2 -
                                                                                        buffer(w * 2 - stride * 2 +
                                                                                               2))) / 6);
          mapCtxs[++j] = clip((W + NEE) / 2 + p1 - (Wp1 + buffer(w - stride * 2 + 1)) / 2);
          mapCtxs[++j] = clip((W + NEE) / 2 + p2 - (Wp2 + buffer(w - stride * 2 + 2)) / 2);
          mapCtxs[++j] = clip((WW + clip(NEE * 2 - NNEE)) / 2 + p1 -
                              (WWp1 + clip(buffer(w - stride * 2 + 1) * 2 - buffer(w * 2 - stride * 2 + 1))) / 2);
          mapCtxs[++j] = clip((WW + clip(NEE * 2 - NNEE)) / 2 + p2 -
                              (WWp2 + clip(buffer(w - stride * 2 + 2) * 2 - buffer(w * 2 - stride * 2 + 2))) / 2);
          mapCtxs[++j] = clip(WW + NEE - N + p1 - clip(WWp1 + buffer(w - stride * 2 + 1) - Np1));
          mapCtxs[++j] = clip(WW + NEE - N + p2 - clip(WWp2 + buffer(w - stride * 2 + 2) - Np2));
          mapCtxs[++j] = clip(W + N - NW);
          mapCtxs[++j] = clip(W + N - NW + p1 - clip(Wp1 + Np1 - NWp1));
          mapCtxs[++j] = clip(W + N - NW + p2 - clip(Wp2 + Np2 - NWp2));
          mapCtxs[++j] = clip(W + NE - N);
          mapCtxs[++j] = clip(N + NW - NNW);
          mapCtxs[++j] = clip(N + NW - NNW + p1 - clip(Np1 + NWp1 - buffer(w * 2 + stride + 1)));
          mapCtxs[++j] = clip(N + NW - NNW + p2 - clip(Np2 + NWp2 - buffer(w * 2 + stride + 2)));
          mapCtxs[++j] = clip(N + NE - NNE);
          mapCtxs[++j] = clip(N + NE - NNE + p1 - clip(Np1 + NEp1 - buffer(w * 2 - stride + 1)));
          mapCtxs[++j] = clip(N + NE - NNE + p2 - clip(Np2 + NEp2 - buffer(w * 2 - stride + 2)));
          mapCtxs[++j] = clip(N + NN - NNN);
          mapCtxs[++j] = clip(N + NN - NNN + p1 - clip(Np1 + NNp1 - buffer(w * 3 + 1)));
          mapCtxs[++j] = clip(N + NN - NNN + p2 - clip(Np2 + NNp2 - buffer(w * 3 + 2)));
          mapCtxs[++j] = clip(W + WW - WWW);
          mapCtxs[++j] = clip(W + WW - WWW + p1 - clip(Wp1 + WWp1 - buffer(stride * 3 + 1)));
          mapCtxs[++j] = clip(W + WW - WWW + p2 - clip(Wp2 + WWp2 - buffer(stride * 3 + 2)));
          mapCtxs[++j] = clip(W + NEE - NE);
          mapCtxs[++j] = clip(W + NEE - NE + p1 - clip(Wp1 + buffer(w - stride * 2 + 1) - NEp1));
          mapCtxs[++j] = clip(W + NEE - NE + p2 - clip(Wp2 + buffer(w - stride * 2 + 2) - NEp2));
          mapCtxs[++j] = clip(NN + p1 - NNp1);
          mapCtxs[++j] = clip(NN + p2 - NNp2);
          mapCtxs[++j] = clip(NN + W - NNW);
          mapCtxs[++j] = clip(NN + W - NNW + p1 - clip(NNp1 + Wp1 - buffer(w * 2 + stride + 1)));
          mapCtxs[++j] = clip(NN + W - NNW + p2 - clip(NNp2 + Wp2 - buffer(w * 2 + stride + 2)));
          mapCtxs[++j] = clip(NN + NW - NNNW);
          mapCtxs[++j] = clip(NN + NW - NNNW + p1 - clip(NNp1 + NWp1 - buffer(w * 3 + stride + 1)));
          mapCtxs[++j] = clip(NN + NW - NNNW + p2 - clip(NNp2 + NWp2 - buffer(w * 3 + stride + 2)));
          mapCtxs[++j] = clip(NN + NE - NNNE);
          mapCtxs[++j] = clip(NN + NE - NNNE + p1 - clip(NNp1 + NEp1 - buffer(w * 3 - stride + 1)));
          mapCtxs[++j] = clip(NN + NE - NNNE + p2 - clip(NNp2 + NEp2 - buffer(w * 3 - stride + 2)));
          mapCtxs[++j] = clip(NN + NNNN - NNNNNN);
          mapCtxs[++j] = clip(NN + NNNN - NNNNNN + p1 - clip(NNp1 + buffer(w * 4 + 1) - buffer(w * 6 + 1)));
          mapCtxs[++j] = clip(NN + NNNN - NNNNNN + p2 - clip(NNp2 + buffer(w * 4 + 2) - buffer(w * 6 + 2)));
          mapCtxs[++j] = clip(WW + p1 - WWp1);
          mapCtxs[++j] = clip(WW + p2 - WWp2);
          mapCtxs[++j] = clip(WW + WWWW - WWWWWW);
          mapCtxs[++j] = clip(WW + WWWW - WWWWWW + p1 - clip(WWp1 + buffer(stride * 4 + 1) - buffer(stride * 6 + 1)));
          mapCtxs[++j] = clip(WW + WWWW - WWWWWW + p2 - clip(WWp2 + buffer(stride * 4 + 2) - buffer(stride * 6 + 2)));
          mapCtxs[++j] = clip(N * 2 - NN + p1 - clip(Np1 * 2 - NNp1));
          mapCtxs[++j] = clip(N * 2 - NN + p2 - clip(Np2 * 2 - NNp2));
          mapCtxs[++j] = clip(W * 2 - WW + p1 - clip(Wp1 * 2 - WWp1));
          mapCtxs[++j] = clip(W * 2 - WW + p2 - clip(Wp2 * 2 - WWp2));
          mapCtxs[++j] = clip(N * 3 - NN * 3 + NNN);
          mapCtxs[++j] = clamp4(N * 3 - NN * 3 + NNN, W, NW, N, NE);
          mapCtxs[++j] = clamp4(W * 3 - WW * 3 + WWW, W, NW, N, NE);
          mapCtxs[++j] = clamp4(N * 2 - NN, W, NW, N, NE);
          mapCtxs[++j] = clip((NNNNN - 6 * NNNN + 15 * NNN - 20 * NN + 15 * N +
                               clamp4(W * 4 - NWW * 6 + NNWWW * 4 - buffer(w * 3 + 4 * stride), W, NW, N, NN)) / 6);
          mapCtxs[++j] = clip(
                  (buffer(w * 3 - 3 * stride) - 4 * NNEE + 6 * NE + clip(W * 4 - NW * 6 + NNW * 4 - NNNW)) / 4);
          mapCtxs[++j] = clip(
                  ((N + 3 * NW) / 4) * 3 - ((NNW + NNWW) / 2) * 3 + (NNNWW * 3 + buffer(w * 3 + 3 * stride)) / 4);
          mapCtxs[++j] = clip((W * 2 + NW) - (WW + 2 * NWW) + NWWW);
          mapCtxs[++j] = (clip(W * 2 - NW) + clip(W * 2 - NWW) + N + NE) / 4;
          mapCtxs[++j] = NNNNNN;
          mapCtxs[++j] = (NEEEE + buffer(w - 6 * stride)) / 2;
          mapCtxs[++j] = (WWWWWW + WWWW) / 2;
          mapCtxs[++j] = ((W + N) * 3 - NW * 2) / 4;
          mapCtxs[++j] = N;
          mapCtxs[++j] = NN;
          assert(++j == nSM1);
          j = -1;
          scMapCtxs[++j] = N + p1 - Np1;
          scMapCtxs[++j] = N + p2 - Np2;
          scMapCtxs[++j] = W + p1 - Wp1;
          scMapCtxs[++j] = W + p2 - Wp2;
          scMapCtxs[++j] = NW + p1 - NWp1;
          scMapCtxs[++j] = NW + p2 - NWp2;
          scMapCtxs[++j] = NE + p1 - NEp1;
          scMapCtxs[++j] = NE + p2 - NEp2;
          scMapCtxs[++j] = NN + p1 - NNp1;
          scMapCtxs[++j] = NN + p2 - NNp2;
          scMapCtxs[++j] = WW + p1 - WWp1;
          scMapCtxs[++j] = WW + p2 - WWp2;
          scMapCtxs[++j] = W + N - NW;
          scMapCtxs[++j] = W + N - NW + p1 - Wp1 - Np1 + NWp1;
          scMapCtxs[++j] = W + N - NW + p2 - Wp2 - Np2 + NWp2;
          scMapCtxs[++j] = W + NE - N;
          scMapCtxs[++j] = W + NE - N + p1 - Wp1 - NEp1 + Np1;
          scMapCtxs[++j] = W + NE - N + p2 - Wp2 - NEp2 + Np2;
          scMapCtxs[++j] = W + NEE - NE;
          scMapCtxs[++j] = W + NEE - NE + p1 - Wp1 - buffer(w - stride * 2 + 1) + NEp1;
          scMapCtxs[++j] = W + NEE - NE + p2 - Wp2 - buffer(w - stride * 2 + 2) + NEp2;
          scMapCtxs[++j] = N + NN - NNN;
          scMapCtxs[++j] = N + NN - NNN + p1 - Np1 - NNp1 + buffer(w * 3 + 1);
          scMapCtxs[++j] = N + NN - NNN + p2 - Np2 - NNp2 + buffer(w * 3 + 2);
          scMapCtxs[++j] = N + NE - NNE;
          scMapCtxs[++j] = N + NE - NNE + p1 - Np1 - NEp1 + buffer(w * 2 - stride + 1);
          scMapCtxs[++j] = N + NE - NNE + p2 - Np2 - NEp2 + buffer(w * 2 - stride + 2);
          scMapCtxs[++j] = N + NW - NNW;
          scMapCtxs[++j] = N + NW - NNW + p1 - Np1 - NWp1 + buffer(w * 2 + stride + 1);
          scMapCtxs[++j] = N + NW - NNW + p2 - Np2 - NWp2 + buffer(w * 2 + stride + 2);
          scMapCtxs[++j] = NE + NW - NN;
          scMapCtxs[++j] = NE + NW - NN + p1 - NEp1 - NWp1 + NNp1;
          scMapCtxs[++j] = NE + NW - NN + p2 - NEp2 - NWp2 + NNp2;
          scMapCtxs[++j] = NW + W - NWW;
          scMapCtxs[++j] = NW + W - NWW + p1 - NWp1 - Wp1 + buffer(w + stride * 2 + 1);
          scMapCtxs[++j] = NW + W - NWW + p2 - NWp2 - Wp2 + buffer(w + stride * 2 + 2);
          scMapCtxs[++j] = W * 2 - WW;
          scMapCtxs[++j] = W * 2 - WW + p1 - Wp1 * 2 + WWp1;
          scMapCtxs[++j] = W * 2 - WW + p2 - Wp2 * 2 + WWp2;
          scMapCtxs[++j] = N * 2 - NN;
          scMapCtxs[++j] = N * 2 - NN + p1 - Np1 * 2 + NNp1;
          scMapCtxs[++j] = N * 2 - NN + p2 - Np2 * 2 + NNp2;
          scMapCtxs[++j] = NW * 2 - NNWW;
          scMapCtxs[++j] = NW * 2 - NNWW + p1 - NWp1 * 2 + buffer(w * 2 + stride * 2 + 1);
          scMapCtxs[++j] = NW * 2 - NNWW + p2 - NWp2 * 2 + buffer(w * 2 + stride * 2 + 2);
          scMapCtxs[++j] = NE * 2 - NNEE;
          scMapCtxs[++j] = NE * 2 - NNEE + p1 - NEp1 * 2 + buffer(w * 2 - stride * 2 + 1);
          scMapCtxs[++j] = NE * 2 - NNEE + p2 - NEp2 * 2 + buffer(w * 2 - stride * 2 + 2);
          scMapCtxs[++j] = N * 3 - NN * 3 + NNN + p1 - Np1 * 3 + NNp1 * 3 - buffer(w * 3 + 1);
          scMapCtxs[++j] = N * 3 - NN * 3 + NNN + p2 - Np2 * 3 + NNp2 * 3 - buffer(w * 3 + 2);
          scMapCtxs[++j] = N * 3 - NN * 3 + NNN;
          scMapCtxs[++j] = (W + NE * 2 - NNE) / 2;
          scMapCtxs[++j] = (W + NE * 3 - NNE * 3 + NNNE) / 2;
          scMapCtxs[++j] = (W + NE * 2 - NNE) / 2 + p1 - (Wp1 + NEp1 * 2 - buffer(w * 2 - stride + 1)) / 2;
          scMapCtxs[++j] = (W + NE * 2 - NNE) / 2 + p2 - (Wp2 + NEp2 * 2 - buffer(w * 2 - stride + 2)) / 2;
          scMapCtxs[++j] = NNE + NE - NNNE;
          scMapCtxs[++j] = NNE + W - NN;
          scMapCtxs[++j] = NNW + W - NNWW;
          scMapCtxs[++j] = 0;
          assert(++j == nSSM);
          j = 0;
          for( int k = (color > 0) ? color - 1 : stride - 1; j < nOLS; j++ ) {
            pOLS[j] = clip(int(floor(ols[j][color].predict(ols_ctxs[j]))));
            ols[j][k].update(p1);
          }

          if( !isPNG ) {
            int mean = W + NW + N + NE;
            const int var = (W * W + NW * NW + N * N + NE * NE - mean * mean / 4) >> 2;
            mean >>= 2;
            const int logvar = ilog(var);

            uint64_t i = 0;
            cm.set(hash(++i, (N + 1) >> 1, logMeanDiffQt(N, clip(NN * 2 - NNN))));
            cm.set(hash(++i, (W + 1) >> 1, logMeanDiffQt(W, clip(WW * 2 - WWW))));
            cm.set(hash(++i, clamp4(W + N - NW, W, NW, N, NE), logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW))));
            cm.set(hash(++i, (NNN + N + 4) / 8, clip(N * 3 - NN * 3 + NNN) >> 1));
            cm.set(hash(++i, (WWW + W + 4) / 8, clip(W * 3 - WW * 3 + WWW) >> 1));
            cm.set(hash(++i, color, (W + clip(NE * 3 - NNE * 3 + NNNE)) / 4, logMeanDiffQt(N, (NW + NE) / 2)));
            cm.set(hash(++i, color, clip((-WWWW + 5 * WWW - 10 * WW + 10 * W +
                                          clamp4(NE * 4 - NNE * 6 + NNNE * 4 - NNNNE, N, NE, NEE, NEEE)) / 5) / 4));
            cm.set(hash(++i, clip(NEE + N - NNEE), logMeanDiffQt(W, clip(NW + NE - NNE))));
            cm.set(hash(++i, clip(NN + W - NNW), logMeanDiffQt(W, clip(NNW + WW - NNWW))));
            cm.set(hash(++i, color, p1));
            cm.set(hash(++i, color, p2));
            cm.set(hash(++i, color, clip(W + N - NW) / 2, clip(W + p1 - Wp1) / 2));
            cm.set(hash(++i, clip(N * 2 - NN) / 2, logMeanDiffQt(N, clip(NN * 2 - NNN))));
            cm.set(hash(++i, clip(W * 2 - WW) / 2, logMeanDiffQt(W, clip(WW * 2 - WWW))));
            cm.set(hash(++i, clamp4(N * 3 - NN * 3 + NNN, W, NW, N, NE) / 2));
            cm.set(hash(++i, clamp4(W * 3 - WW * 3 + WWW, W, N, NE, NEE) / 2));
            cm.set(hash(++i, color, logMeanDiffQt(W, Wp1), clamp4((p1 * W) / (Wp1 < 1 ? 1 : Wp1), W, N, NE,
                                                                  NEE))); //using max(1,Wp1) results in division by zero in VC2015
            cm.set(hash(++i, color, clamp4(N + p2 - Np2, W, NW, N, NE)));
            cm.set(hash(++i, color, clip(W + N - NW), column[0]));
            cm.set(hash(++i, color, clip(N * 2 - NN), logMeanDiffQt(W, clip(NW * 2 - NNW))));
            cm.set(hash(++i, color, clip(W * 2 - WW), logMeanDiffQt(N, clip(NW * 2 - NWW))));
            cm.set(hash(++i, (W + NEE) / 2, logMeanDiffQt(W, (WW + NE) / 2)));
            cm.set(hash(++i, (clamp4(clip(W * 2 - WW) + clip(N * 2 - NN) - clip(NW * 2 - NNWW), W, NW, N, NE))));
            cm.set(hash(++i, color, W, p2));
            cm.set(hash(++i, N, NN & 0x1F, NNN & 0x1F));
            cm.set(hash(++i, W, WW & 0x1F, WWW & 0x1F));
            cm.set(hash(++i, color, N, column[0]));
            cm.set(hash(++i, color, clip(W + NEE - NE), logMeanDiffQt(W, clip(WW + NE - N))));
            cm.set(hash(++i, NN, NNNN & 0x1F, NNNNNN & 0x1F, column[1]));
            cm.set(hash(++i, WW, WWWW & 0x1F, WWWWWW & 0x1F, column[1]));
            cm.set(hash(++i, NNN, NNNNNN & 0x1F, buffer(w * 9) & 0x1F, column[1]));
            cm.set(hash(++i, color, column[1]));

            cm.set(hash(++i, color, W, logMeanDiffQt(W, WW)));
            cm.set(hash(++i, color, W, p1));
            cm.set(hash(++i, color, W / 4, logMeanDiffQt(W, p1), logMeanDiffQt(W, p2)));
            cm.set(hash(++i, color, N, logMeanDiffQt(N, NN)));
            cm.set(hash(++i, color, N, p1));
            cm.set(hash(++i, color, N / 4, logMeanDiffQt(N, p1), logMeanDiffQt(N, p2)));
            cm.set(hash(++i, color, (W + N) >> 3, p1 >> 4, p2 >> 4));
            cm.set(hash(++i, color, p1 / 2, p2 / 2));
            cm.set(hash(++i, color, W, p1 - Wp1));
            cm.set(hash(++i, color, W + p1 - Wp1));
            cm.set(hash(++i, color, N, p1 - Np1));
            cm.set(hash(++i, color, N + p1 - Np1));
            cm.set(hash(++i, color, mean, logvar >> 4));

            ctx[0] = (min(color, stride - 1) << 9) | ((abs(W - N) > 3) << 8) | ((W > N) << 7) | ((W > NW) << 6) |
                     ((abs(N - NW) > 3) << 5) | ((N > NW) << 4) | ((abs(N - NE) > 3) << 3) | ((N > NE) << 2) |
                     ((W > WW) << 1) | (N > NN);
            ctx[1] = ((logMeanDiffQt(p1, clip(Np1 + NEp1 - buffer(w * 2 - stride + 1))) >> 1) << 5) |
                     ((logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW)) >> 1) << 2) | min(color, stride - 1);
          } else {
            int residuals[5] = {((int8_t) buf(stride + (x <= stride))) + 128, ((int8_t) buf(1 + (x < 2))) + 128,
                                ((int8_t) buf(stride + 1 + (x <= stride))) + 128, ((int8_t) buf(2 + (x < 3))) + 128,
                                ((int8_t) buf(stride + 2 + (x <= stride))) + 128};
            R1 = (residuals[1] * residuals[0]) / max(1, residuals[2]);
            R2 = (residuals[3] * residuals[0]) / max(1, residuals[4]);

            uint64_t i = (filterOn ? filter + 1 : 0) * 1024;
            cm.set(0);
            cm.set(hash(++i, color, clip(W + N - NW) - px, clip(W + p1 - Wp1) - px, R1));
            cm.set(hash(++i, color, clip(W + N - NW) - px, logMeanDiffQt(p1, clip(Wp1 + Np1 - NWp1))));
            cm.set(hash(++i, color, clip(W + N - NW) - px, logMeanDiffQt(p2, clip(Wp2 + Np2 - NWp2)), R2 / 4));
            cm.set(hash(++i, color, clip(W + N - NW) - px, clip(N + NE - NNE) - clip(N + NW - NNW)));
            cm.set(hash(++i, color, clip(W + N - NW + p1 - (Wp1 + Np1 - NWp1)), px, R1));
            cm.set(hash(++i, color, clamp4(W + N - NW, W, NW, N, NE) - px, column[0]));
            cm.set(hash(++i, clamp4(W + N - NW, W, NW, N, NE) / 8, px));
            cm.set(hash(++i, color, N - px, clip(N + p1 - Np1) - px));
            cm.set(hash(++i, color, clip(W + p1 - Wp1) - px, R1));
            cm.set(hash(++i, color, clip(N + p1 - Np1) - px));
            cm.set(hash(++i, color, clip(N + p1 - Np1) - px, clip(N + p2 - Np2) - px));
            cm.set(hash(++i, color, clip(W + p1 - Wp1) - px, clip(W + p2 - Wp2) - px));
            cm.set(hash(++i, color, clip(NW + p1 - NWp1) - px));
            cm.set(hash(++i, color, clip(NW + p1 - NWp1) - px, column[0]));
            cm.set(hash(++i, color, clip(NE + p1 - NEp1) - px, column[0]));
            cm.set(hash(++i, color, clip(NE + N - NNE) - px, clip(NE + p1 - NEp1) - px));
            cm.set(hash(++i, clip(N + NE - NNE) - px, column[0]));
            cm.set(hash(++i, color, clip(NW + N - NNW) - px, clip(NW + p1 - NWp1) - px));
            cm.set(hash(++i, clip(N + NW - NNW) - px, column[0]));
            cm.set(hash(++i, clip(NN + W - NNW) - px, logMeanDiffQt(N, clip(NNN + NW - NNNW))));
            cm.set(hash(++i, clip(W + NEE - NE) - px, logMeanDiffQt(W, clip(WW + NE - N))));
            cm.set(hash(++i, color, clip(N + NN - NNN + buffer(1 + (!color)) -
                                         clip(buffer(w + 1 + (!color)) + buffer(w * 2 + 1 + (!color)) -
                                              buffer(w * 3 + 1 + (!color)))) - px));
            cm.set(hash(++i, clip(N + NN - NNN) - px, clip(5 * N - 10 * NN + 10 * NNN - 5 * NNNN + NNNNN) - px));
            cm.set(hash(++i, color, clip(N * 2 - NN) - px, logMeanDiffQt(N, clip(NN * 2 - NNN))));
            cm.set(hash(++i, color, clip(W * 2 - WW) - px, logMeanDiffQt(W, clip(WW * 2 - WWW))));
            cm.set(hash(++i, clip(N * 3 - NN * 3 + NNN) - px));
            cm.set(hash(++i, color, clip(N * 3 - NN * 3 + NNN) - px, logMeanDiffQt(W, clip(NW * 2 - NNW))));
            cm.set(hash(++i, clip(W * 3 - WW * 3 + WWW) - px));
            cm.set(hash(++i, color, clip(W * 3 - WW * 3 + WWW) - px, logMeanDiffQt(N, clip(NW * 2 - NWW))));
            cm.set(hash(++i, clip((35 * N - 35 * NNN + 21 * NNNNN - 5 * buffer(w * 7)) / 16) - px));
            cm.set(hash(++i, color, (W + clip(NE * 3 - NNE * 3 + NNNE)) / 2 - px, R2));
            cm.set(hash(++i, color, (W + clamp4(NE * 3 - NNE * 3 + NNNE, W, N, NE, NEE)) / 2 - px,
                        logMeanDiffQt(N, (NW + NE) / 2)));
            cm.set(hash(++i, color, (W + NEE) / 2 - px, R1 / 2));
            cm.set(hash(++i, color,
                        clamp4(clip(W * 2 - WW) + clip(N * 2 - NN) - clip(NW * 2 - NNWW), W, NW, N, NE) - px));
            cm.set(hash(++i, color, buf(stride + (x <= stride)), buf(1 + (x < 2)), buf(2 + (x < 3))));
            cm.set(hash(++i, color, buf(1 + (x < 2)), px));
            cm.set(hash(++i, buf(w + 1), buf((w + 1) * 2), buf((w + 1) * 3), px));
            ctx[0] = (min(color, stride - 1) << 9) | ((abs(W - N) > 3) << 8) | ((W > N) << 7) | ((W > NW) << 6) |
                     ((abs(N - NW) > 3) << 5) | ((N > NW) << 4) | ((N > NE) << 3) | min(5, filterOn ? filter + 1 : 0);
            ctx[1] = ((logMeanDiffQt(p1, clip(Np1 + NEp1 - buffer(w * 2 - stride + 1))) >> 1) << 5) |
                     ((logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW)) >> 1) << 2) | min(color, stride - 1);
          }

          int i = -1;
          map[++i].set_direct((W & 0xC0) | ((N & 0xC0) >> 2) | ((WW & 0xC0) >> 4) | (NN >> 6));
          map[++i].set_direct((N & 0xC0) | ((NN & 0xC0) >> 2) | ((NE & 0xC0) >> 4) | (NEE >> 6));
          map[++i].set_direct(buf(1 + (isPNG && x < 2)));
          map[++i].set_direct(min(color, stride - 1));
          map[++i].set_direct(0);
          stats->Image.plane = min(color, stride - 1);
          stats->Image.pixels.W = W;
          stats->Image.pixels.N = N;
          stats->Image.pixels.NN = NN;
          stats->Image.pixels.WW = WW;
          stats->Image.pixels.Wp1 = Wp1;
          stats->Image.pixels.Np1 = Np1;
          stats->Image.ctx = ctx[0] >> 3;
        }
      }
      if( x > 0 || !isPNG ) {
        INJECT_SHARED_c0
        uint8_t B = (c0 << (8 - bpos));
        int i = 4;

        map[++i].set_direct((((uint8_t) (clip(W + N - NW) - px - B)) * 8 + bpos) |
                           (logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW)) << 11));
        map[++i].set_direct(
                (((uint8_t) (clip(N * 2 - NN) - px - B)) * 8 + bpos) | (logMeanDiffQt(W, clip(NW * 2 - NNW)) << 11));
        map[++i].set_direct(
                (((uint8_t) (clip(W * 2 - WW) - px - B)) * 8 + bpos) | (logMeanDiffQt(N, clip(NW * 2 - NWW)) << 11));
        map[++i].set_direct((((uint8_t) (clip(W + N - NW) - px - B)) * 8 + bpos) |
                           (logMeanDiffQt(p1, clip(Wp1 + Np1 - NWp1)) << 11));
        map[++i].set_direct((((uint8_t) (clip(W + N - NW) - px - B)) * 8 + bpos) |
                           (logMeanDiffQt(p2, clip(Wp2 + Np2 - NWp2)) << 11));
        map[++i].set(hash(W - px - B, N - px - B) * 8 + bpos);
        map[++i].set(hash(W - px - B, WW - px - B) * 8 + bpos);
        map[++i].set(hash(N - px - B, NN - px - B) * 8 + bpos);
        map[++i].set(hash(clip(N + NE - NNE) - px - B, clip(N + NW - NNW) - px - B) * 8 + bpos);
        map[++i].set_direct((min(color, stride - 1) << 11) | (((uint8_t) (clip(N + p1 - Np1) - px - B)) * 8 + bpos));
        map[++i].set_direct((min(color, stride - 1) << 11) | (((uint8_t) (clip(N + p2 - Np2) - px - B)) * 8 + bpos));
        map[++i].set_direct((min(color, stride - 1) << 11) | (((uint8_t) (clip(W + p1 - Wp1) - px - B)) * 8 + bpos));
        map[++i].set_direct((min(color, stride - 1) << 11) | (((uint8_t) (clip(W + p2 - Wp2) - px - B)) * 8 + bpos));
        ++i;
        assert(i == nSM0);

        for( int j = 0; j < nSM1; i++, j++ )
          map[i].set_direct((mapCtxs[j] - px - B) * 8 + bpos);

        for( int j = 0; i < nSM; i++, j++ )
          map[i].set_direct((pOLS[j] - px - B) * 8 + bpos);

        for( int i = 0; i < nSSM; i++ )
          SCMap[i].set((scMapCtxs[i] - px - B) * 8 + bpos);
      }
    }

    void init() { //new image
      stride = 3 + alpha;
      w = info & 0xFFFFFF;
      padding = w % stride;
      x = color = line = px = 0;
      filterOn = false;
      columns[0] = max(1, w / max(1, ilog2(w) * 3));
      columns[1] = max(1, columns[0] / max(1, ilog2(columns[0])));
      if( lastPos > 0 && lastWasPNG != isPNG ) {
        for( int i = 0; i < nSM; i++ )
          map[i].reset(0);
      }
      lastWasPNG = isPNG;
      buffer.fill(0x7F);
    }

    void setParam(int info0, uint32_t alpha0, uint32_t isPNG0) {
      info = info0;
      alpha = alpha0;
      isPNG = isPNG0;
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      INJECT_SHARED_pos
      if( bpos == 0 ) {
        if((color < 0) || (pos - lastPos != 1))
          init();
        else {
          x++;
          if( x >= w + (int) isPNG ) {
            x = 0;
            line++;
          }
        }
        lastPos = pos;
      }

      update();

      // predict next bit
      if( x > 0 || !isPNG ) {
        cm.mix(m);

        for( int i = 0; i < nSM; i++ )
          map[i].mix(m);

        for( int i = 0; i < nSSM; i++ )
          SCMap[i].mix(m);

        if( ++col >= stride * 8 )
          col = 0;
        m.set(5, 6);
        m.set(min(63, column[0]) + ((ctx[0] >> 3) & 0xC0), 256);
        m.set(min(127, column[1]) + ((ctx[0] >> 2) & 0x180), 512);
        INJECT_SHARED_c0
        m.set((ctx[0] & 0x7FC) | (bpos >> 1), 2048);
        m.set(col + (isPNG ? (ctx[0] & 7) + 1 : (c0 == ((0x100 | ((N + W) / 2)) >> (8 - bpos)))) * 32, 8 * 32);
        m.set(((isPNG ? p1 : 0) >> 4) * stride + (x % stride) + min(5, filterOn ? filter + 1 : 0) * 64, 6 * 64);
        m.set(c0 + 256 * (isPNG && abs(R1 - 128) > 8), 256 * 2);
        m.set((ctx[1] << 2) | (bpos >> 1), 1024);
        m.set(finalize64(
                hash(logMeanDiffQt(W, WW, 5), logMeanDiffQt(N, NN, 5), logMeanDiffQt(W, N, 5), ilog2(W), color), 13),
              8192);
        m.set(finalize64(hash(ctx[0], column[0] / 8), 13), 8192);
        m.set(finalize64(hash(logQt(N, 5), logMeanDiffQt(N, NN, 3), c0), 13), 8192);
        m.set(finalize64(hash(logQt(W, 5), logMeanDiffQt(W, WW, 3), c0), 13), 8192);
        m.set(min(255, (x + line) / 32), 256);
      } else {
        m.add(-2048 + ((filter >> (7 - bpos)) & 1) * 4096);
        m.set(min(4, filter), MIXERCONTEXTSETS);
      }
    }
};

#endif //PAQ8PX_IMAGE24BITMODEL_HPP
