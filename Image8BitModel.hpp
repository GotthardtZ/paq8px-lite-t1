#ifndef PAQ8PX_IMAGE8BITMODEL_HPP
#define PAQ8PX_IMAGE8BITMODEL_HPP

/**
 * Model for 8-bit image data
 */
class Image8bitModel {
private:
    static constexpr int nSM0 = 2;
    static constexpr int nSM1 = 55;
    static constexpr int nOLS = 5;
    static constexpr int nSM = nSM0 + nSM1 + nOLS;
    static constexpr int nPltMaps = 4;
    static constexpr int nCM = nPltMaps + 49;

public:
    static constexpr int MIXERINPUTS =
            nSM * StationaryMap::MIXERINPUTS + nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS);
    static constexpr int MIXERCONTEXTS = (2048 + 5) + 6 * 16 + 6 * 32 + 256 + 1024 + 64 + 128 + 256; // 4069
    static constexpr int MIXERCONTEXTSETS = 8;

    const Shared *const shared;
    ModelStats *stats;
    ContextMap2 cm;
    StationaryMap map[nSM];
    SmallStationaryContextMap pltMap[nPltMaps];
    IndirectMap sceneMap[5];
    IndirectContext<uint8_t> iCtx[nPltMaps];
    Buf buffer {0x100000}; // internal rotating buffer for (PNG unfiltered) pixel data
    Array<short> jumps {0x8000};
    //pixel neighborhood
    uint8_t WWWWWW = 0, WWWWW = 0, WWWW = 0, WWW = 0, WW = 0, W = 0;
    uint8_t NWWWW = 0, NWWW = 0, NWW = 0, NW = 0, N = 0, NE = 0, NEE = 0, NEEE = 0, NEEEE = 0;
    uint8_t NNWWW = 0, NNWW = 0, NNW = 0, NN = 0, NNE = 0, NNEE = 0, NNEEE = 0;
    uint8_t NNNWW = 0, NNNW = 0, NNN = 0, NNNE = 0, NNNEE = 0;
    uint8_t NNNNW = 0, NNNN = 0, NNNNE = 0;
    uint8_t NNNNN = 0;
    uint8_t NNNNNN = 0;
    uint8_t px = 0, res = 0, prvFrmPx = 0, prvFrmPrediction = 0; // current PNG filter prediction, expected residual, corresponding pixel in previous frame
    uint32_t lastPos = 0, lastWasPNG = 0;
    uint32_t gray = 0, isPNG = 0;
    int w = 0, ctx = 0, col = 0, line = 0, x = 0, filter = 0, jump = 0;
    int framePos = 0, prevFramePos = 0, frameWidth = 0, prevFrameWidth = 0;
    bool filterOn = false;
    int columns[2] = {1, 1}, column[2] {};
    uint8_t MapCtxs[nSM1] = {0}, pOLS[nOLS] = {0};
    static constexpr double lambda[nOLS] = {0.996, 0.87, 0.93, 0.8, 0.9};
    static constexpr int num[nOLS] = {32, 12, 15, 10, 14};
    OLS<double, uint8_t> ols[nOLS] = {{num[0], 1, lambda[0]},
                                      {num[1], 1, lambda[1]},
                                      {num[2], 1, lambda[2]},
                                      {num[3], 1, lambda[3]},
                                      {num[4], 1, lambda[4]}};
    OLS<double, uint8_t> sceneOls {13, 1, 0.994};
    const uint8_t *ols_ctx1[32] = {&WWWWWW, &WWWWW, &WWWW, &WWW, &WW, &W, &NWWWW, &NWWW, &NWW, &NW, &N, &NE, &NEE, &NEEE, &NEEEE, &NNWWW,
                                   &NNWW, &NNW, &NN, &NNE, &NNEE, &NNEEE, &NNNWW, &NNNW, &NNN, &NNNE, &NNNEE, &NNNNW, &NNNN, &NNNNE, &NNNNN,
                                   &NNNNNN};
    const uint8_t *ols_ctx2[12] = {&WWW, &WW, &W, &NWW, &NW, &N, &NE, &NEE, &NNW, &NN, &NNE, &NNN};
    const uint8_t *ols_ctx3[15] = {&N, &NE, &NEE, &NEEE, &NEEEE, &NN, &NNE, &NNEE, &NNEEE, &NNN, &NNNE, &NNNEE, &NNNN, &NNNNE, &NNNNN};
    const uint8_t *ols_ctx4[10] = {&N, &NE, &NEE, &NEEE, &NN, &NNE, &NNEE, &NNN, &NNNE, &NNNN};
    const uint8_t *ols_ctx5[14] = {&WWWW, &WWW, &WW, &W, &NWWW, &NWW, &NW, &N, &NNWW, &NNW, &NN, &NNNW, &NNN, &NNNN};
    const uint8_t **ols_ctxs[nOLS] = {&ols_ctx1[0], &ols_ctx2[0], &ols_ctx3[0], &ols_ctx4[0], &ols_ctx5[0]};

public:
    Image8bitModel(const Shared *const sh, ModelStats *st, const uint64_t size) : shared(sh), stats(st),
                                                                                  cm(sh, size, nCM, 64, CM_USE_RUN_STATS),
                                                                                  map {/* StationaryMap: BitsOfContext, InputBits, Scale, Limit  */
                                                                                          /*nSM0: 0- 1*/ {sh, 0,  8, 64, 1023},
                                                                                                         {sh, 15, 1, 64, 1023},
                                                                                          /*nSM1: 0- 4*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1: 5- 9*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:10-14*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:15-19*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:20-24*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:25-29*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:30-34*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:35-39*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:40-44*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:45-49*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nSM1:50-54*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                          /*nOLS:   0- 4*/
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023},
                                                                                                         {sh, 11, 1, 64, 1023}},
                                                                                  pltMap {/* SmallStationaryContextMap: BitsOfContext, InputBits, Rate, Scale */
                                                                                          {sh, 11, 1, 7, 64},
                                                                                          {sh, 11, 1, 7, 64},
                                                                                          {sh, 11, 1, 7, 64},
                                                                                          {sh, 11, 1, 7, 64}},
                                                                                  sceneMap {/* IndirectMap: BitsOfContext, InputBits, Scale, Limit */
                                                                                          {sh, 8,  8, 64, 255},
                                                                                          {sh, 8,  8, 64, 255},
                                                                                          {sh, 22, 1, 64, 255},
                                                                                          {sh, 11, 1, 64, 255},
                                                                                          {sh, 11, 1, 64, 255}},
                                                                                  iCtx {/* IndirectContext<uint8_t>: BitsPerContext, InputBits */
                                                                                          {16, 8},
                                                                                          {16, 8},
                                                                                          {16, 8},
                                                                                          {16, 8}} {}

    void setParam(int info0, uint32_t gray0, uint32_t isPNG0) {
      w = info0;
      gray = gray0;
      isPNG = isPNG0;
    }

    void mix(Mixer &m) {
      // Select nearby pixels as context
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        INJECT_SHARED_c1
        INJECT_SHARED_buf
        INJECT_SHARED_pos
        if( pos != lastPos + 1 ) {
          x = line = px = jump = 0;
          filterOn = false;
          columns[0] = max(1, w / max(1, ilog2(w) * 2));
          columns[1] = max(1, columns[0] / max(1, ilog2(columns[0])));
          if( gray ) {
            if( lastPos && lastWasPNG != isPNG ) {
              for( int i = 0; i < nSM; i++ )
                map[i].reset(0);
            }
            lastWasPNG = isPNG;
          }
          buffer.fill(0x7F);
          prevFramePos = framePos;
          framePos = pos;
          prevFrameWidth = frameWidth;
          frameWidth = w;
        } else {
          x++;
          if( x >= w + (int) isPNG ) {
            x = 0;
            line++;
          }
        }
        lastPos = pos;

        if( isPNG ) {
          if( x == 1 )
            filter = (uint8_t) c1;
          else {
            switch( filter ) {
              case 1: {
                buffer.add((uint8_t) (c1 + buffer(1) * (x > 2 || !x)));
                filterOn = x > 1;
                px = buffer(1);
                break;
              }
              case 2: {
                buffer.add((uint8_t) (c1 + buffer(w) * (filterOn = (line > 0))));
                px = buffer(w);
                break;
              }
              case 3: {
                buffer.add((uint8_t) (c1 + (buffer(w) * (line > 0) + buffer(1) * (x > 2 || !x)) / 2));
                filterOn = (x > 1 || line > 0);
                px = (buffer(1) * (x > 1) + buffer(w) * (line > 0)) / 2;
                break;
              }
              case 4: {
                buffer.add((uint8_t) (c1 + paeth(buffer(1) * (x > 2 || !x), buffer(w) * (line > 0),
                                                 buffer(w + 1) * (line > 0 && (x > 2 || !x)))));
                filterOn = (x > 1 || line > 0);
                px = paeth(buffer(1) * (x > 1), buffer(w) * (line > 0), buffer(w + 1) * (x > 1 && line > 0));
                break;
              }
              default:
                buffer.add(c1);
                filterOn = false;
                px = 0;
            }
            if( !filterOn )
              px = 0;
          }
        } else { // non-png
          buffer.add(c1);
          if( x == 0 ) {
            memset(&jumps[0], 0, sizeof(short) * jumps.size());
            if( line > 0 && w > 8 ) {
              uint8_t bMask = 0xFF - ((1 << gray) - 1);
              uint32_t pMask = bMask * 0x01010101u;
              uint32_t left = 0, right = 0;
              int l = min(w, (int) jumps.size()), end = l - 4;
              do {
                left = ((buffer(l - x) << 24) | (buffer(l - x - 1) << 16) | (buffer(l - x - 2) << 8) | buffer(l - x - 3)) & pMask;
                int i = end;
                while( i >= x + 4 ) {
                  right = ((buffer(l - i - 3) << 24) | (buffer(l - i - 2) << 16) | (buffer(l - i - 1) << 8) | buffer(l - i)) & pMask;
                  if( left == right ) {
                    int j = (i + 3 - x - 1) / 2, k = 0;
                    for( ; k <= j; k++ ) {
                      if( k < 4 || (buffer(l - x - k) & bMask) == (buffer(l - i - 3 + k) & bMask)) {
                        jumps[x + k] = -(x + (l - i - 3) + 2 * k);
                        jumps[i + 3 - k] = i + 3 - x - 2 * k;
                      } else
                        break;
                    }
                    x += k;
                    end -= k;
                    break;
                  }
                  i--;
                }
                x++;
                if( x > end )
                  break;
              } while( x + 4 < l );
              x = 0;
            }
          }
        }

        if( x || !isPNG ) {
          column[0] = (x - isPNG) / columns[0];
          column[1] = (x - isPNG) / columns[1];
          WWWWW = buffer(5), WWWW = buffer(4), WWW = buffer(3), WW = buffer(2), W = buffer(1);
          NWWWW = buffer(w + 4), NWWW = buffer(w + 3), NWW = buffer(w + 2), NW = buffer(w + 1), N = buffer(w), NE = buffer(
                  w - 1), NEE = buffer(w - 2), NEEE = buffer(w - 3), NEEEE = buffer(w - 4);
          NNWWW = buffer(w * 2 + 3), NNWW = buffer(w * 2 + 2), NNW = buffer(w * 2 + 1), NN = buffer(w * 2), NNE = buffer(
                  w * 2 - 1), NNEE = buffer(w * 2 - 2), NNEEE = buffer(w * 2 - 3);
          NNNWW = buffer(w * 3 + 2), NNNW = buffer(w * 3 + 1), NNN = buffer(w * 3), NNNE = buffer(w * 3 - 1), NNNEE = buffer(w * 3 - 2);
          NNNNW = buffer(w * 4 + 1), NNNN = buffer(w * 4), NNNNE = buffer(w * 4 - 1);
          NNNNN = buffer(w * 5);
          NNNNNN = buffer(w * 6);
          if( prevFramePos > 0 && prevFrameWidth == w ) {
            int offset = prevFramePos + line * w + x;
            prvFrmPx = buf[offset];
            if( gray ) {
              sceneOls.update(W);
              sceneOls.add(W);
              sceneOls.add(NW);
              sceneOls.add(N);
              sceneOls.add(NE);
              for( int i = -1; i < 2; i++ ) {
                for( int j = -1; j < 2; j++ )
                  sceneOls.add(buf[offset + i * w + j]);
              }
              prvFrmPrediction = clip(int(floor(sceneOls.predict())));
            } else
              prvFrmPrediction = W;
          } else {
            prvFrmPx = prvFrmPrediction = W;
          }
          sceneMap[0].set_direct(prvFrmPx);
          sceneMap[1].set_direct(prvFrmPrediction);

          int j = 0;
          jump = jumps[min(x, (int) jumps.size() - 1)];
          uint64_t i = (filterOn ? (filter + 1) * 64 : 0) + (gray * 1024);
          cm.set(hash(++i, (jump != 0) ? (0x100 | buffer(abs(jump))) * (1 - 2 * (jump < 0)) : N, line & 3));
          if( !gray ) {
            for( j = 0; j < nPltMaps; j++ )
              iCtx[j] += W;
            iCtx[0] = W | (NE << 8);
            iCtx[1] = W | (N << 8);
            iCtx[2] = W | (WW << 8);
            iCtx[3] = N | (NN << 8);

            cm.set(hash(++i, W, px));
            cm.set(hash(++i, W, px, column[0]));
            cm.set(hash(++i, N, px));
            cm.set(hash(++i, N, px, column[0]));
            cm.set(hash(++i, NW, px));
            cm.set(hash(++i, NW, px, column[0]));
            cm.set(hash(++i, NE, px));
            cm.set(hash(++i, NE, px, column[0]));
            cm.set(hash(++i, NWW, px));
            cm.set(hash(++i, NEE, px));
            cm.set(hash(++i, WW, px));
            cm.set(hash(++i, NN, px));
            cm.set(hash(++i, W, N, px));
            cm.set(hash(++i, W, NW, px));
            cm.set(hash(++i, W, NE, px));
            cm.set(hash(++i, W, NEE, px));
            cm.set(hash(++i, W, NWW, px));
            cm.set(hash(++i, N, NW, px));
            cm.set(hash(++i, N, NE, px));
            cm.set(hash(++i, NW, NE, px));
            cm.set(hash(++i, W, WW, px));
            cm.set(hash(++i, N, NN, px));
            cm.set(hash(++i, NW, NNWW, px));
            cm.set(hash(++i, NE, NNEE, px));
            cm.set(hash(++i, NW, NWW, px));
            cm.set(hash(++i, NW, NNW, px));
            cm.set(hash(++i, NE, NEE, px));
            cm.set(hash(++i, NE, NNE, px));
            cm.set(hash(++i, N, NNW, px));
            cm.set(hash(++i, N, NNE, px));
            cm.set(hash(++i, N, NNN, px));
            cm.set(hash(++i, W, WWW, px));
            cm.set(hash(++i, WW, NEE, px));
            cm.set(hash(++i, WW, NN, px));
            cm.set(hash(++i, W, buffer(w - 3), px));
            cm.set(hash(++i, W, buffer(w - 4), px));
            cm.set(hash(++i, W, N, NW, px));
            cm.set(hash(++i, N, NN, NNN, px));
            cm.set(hash(++i, W, NE, NEE, px));
            cm.set(hash(++i, W, NW, N, NE, px));
            cm.set(hash(++i, N, NE, NN, NNE, px));
            cm.set(hash(++i, N, NW, NNW, NN, px));
            cm.set(hash(++i, W, WW, NWW, NW, px));
            cm.set(hash(++i, W, NW << 8 | N, WW << 8 | NWW, px));
            cm.set(hash(++i, px, column[0]));
            cm.set(hash(++i, px));
            cm.set(hash(++i, N, px, column[1]));
            cm.set(hash(++i, W, px, column[1]));
            for( int j = 0; j < nPltMaps; j++ )
              cm.set(hash(++i, iCtx[j](), px));

            ctx = min(0x1F, (x - isPNG) / min(0x20, columns[0]));
            res = W;
          } else { // gray
            MapCtxs[j++] = clamp4(W + N - NW, W, NW, N, NE);
            MapCtxs[j++] = clip(W + N - NW);
            MapCtxs[j++] = clamp4(W + NE - N, W, NW, N, NE);
            MapCtxs[j++] = clip(W + NE - N);
            MapCtxs[j++] = clamp4(N + NW - NNW, W, NW, N, NE);
            MapCtxs[j++] = clip(N + NW - NNW);
            MapCtxs[j++] = clamp4(N + NE - NNE, W, N, NE, NEE);
            MapCtxs[j++] = clip(N + NE - NNE);
            MapCtxs[j++] = (W + NEE) / 2;
            MapCtxs[j++] = clip(N * 3 - NN * 3 + NNN);
            MapCtxs[j++] = clip(W * 3 - WW * 3 + WWW);
            MapCtxs[j++] = (W + clip(NE * 3 - NNE * 3 + buffer(w * 3 - 1))) / 2;
            MapCtxs[j++] = (W + clip(NEE * 3 - buffer(w * 2 - 3) * 3 + buffer(w * 3 - 4))) / 2;
            MapCtxs[j++] = clip(NN + buffer(w * 4) - buffer(w * 6));
            MapCtxs[j++] = clip(WW + buffer(4) - buffer(6));
            MapCtxs[j++] = clip((buffer(w * 5) - 6 * buffer(w * 4) + 15 * NNN - 20 * NN + 15 * N + clamp4(W * 2 - NWW, W, NW, N, NN)) / 6);
            MapCtxs[j++] = clip(
                    (-3 * WW + 8 * W + clamp4(NEE * 3 - NNEE * 3 + buffer(w * 3 - 2), NE, NEE, buffer(w - 3), buffer(w - 4))) / 6);
            MapCtxs[j++] = clip(NN + NW - buffer(w * 3 + 1));
            MapCtxs[j++] = clip(NN + NE - buffer(w * 3 - 1));
            MapCtxs[j++] = clip((W * 2 + NW) - (WW + 2 * NWW) + buffer(w + 3));
            MapCtxs[j++] = clip(((NW + NWW) / 2) * 3 - buffer(w * 2 + 3) * 3 + (buffer(w * 3 + 4) + buffer(w * 3 + 5)) / 2);
            MapCtxs[j++] = clip(NEE + NE - buffer(w * 2 - 3));
            MapCtxs[j++] = clip(NWW + WW - buffer(w + 4));
            MapCtxs[j++] = clip(((W + NW) * 3 - NWW * 6 + buffer(w + 3) + buffer(w * 2 + 3)) / 2);
            MapCtxs[j++] = clip((NE * 2 + NNE) - (NNEE + buffer(w * 3 - 2) * 2) + buffer(w * 4 - 3));
            MapCtxs[j++] = buffer(w * 6);
            MapCtxs[j++] = (buffer(w - 4) + buffer(w - 6)) / 2;
            MapCtxs[j++] = (buffer(4) + buffer(6)) / 2;
            MapCtxs[j++] = (W + N + buffer(w - 5) + buffer(w - 7)) / 4;
            MapCtxs[j++] = clip(buffer(w - 3) + W - NEE);
            MapCtxs[j++] = clip(4 * NNN - 3 * buffer(w * 4));
            MapCtxs[j++] = clip(N + NN - NNN);
            MapCtxs[j++] = clip(W + WW - WWW);
            MapCtxs[j++] = clip(W + NEE - NE);
            MapCtxs[j++] = clip(WW + NEE - N);
            MapCtxs[j++] = (clip(W * 2 - NW) + clip(W * 2 - NWW) + N + NE) / 4;
            MapCtxs[j++] = clamp4(N * 2 - NN, W, N, NE, NEE);
            MapCtxs[j++] = (N + NNN) / 2;
            MapCtxs[j++] = clip(NN + W - NNW);
            MapCtxs[j++] = clip(NWW + N - NNWW);
            MapCtxs[j++] = clip((4 * WWW - 15 * WW + 20 * W + clip(NEE * 2 - NNEE)) / 10);
            MapCtxs[j++] = clip((buffer(w * 3 - 3) - 4 * NNEE + 6 * NE + clip(W * 3 - NW * 3 + NNW)) / 4);
            MapCtxs[j++] = clip((N * 2 + NE) - (NN + 2 * NNE) + buffer(w * 3 - 1));
            MapCtxs[j++] = clip((NW * 2 + NNW) - (NNWW + buffer(w * 3 + 2) * 2) + buffer(w * 4 + 3));
            MapCtxs[j++] = clip(NNWW + W - buffer(w * 2 + 3));
            MapCtxs[j++] = clip(
                    (-buffer(w * 4) + 5 * NNN - 10 * NN + 10 * N + clip(W * 4 - NWW * 6 + buffer(w * 2 + 3) * 4 - buffer(w * 3 + 4))) / 5);
            MapCtxs[j++] = clip(NEE + clip(buffer(w - 3) * 2 - buffer(w * 2 - 4)) - buffer(w - 4));
            MapCtxs[j++] = clip(NW + W - NWW);
            MapCtxs[j++] = clip((N * 2 + NW) - (NN + 2 * NNW) + buffer(w * 3 + 1));
            MapCtxs[j++] = clip(NN + clip(NEE * 2 - buffer(w * 2 - 3)) - NNE);
            MapCtxs[j++] = clip((-buffer(4) + 5 * WWW - 10 * WW + 10 * W + clip(NE * 2 - NNE)) / 5);
            MapCtxs[j++] = clip((-buffer(5) + 4 * buffer(4) - 5 * WWW + 5 * W + clip(NE * 2 - NNE)) / 4);
            MapCtxs[j++] = clip((WWW - 4 * WW + 6 * W + clip(NE * 3 - NNE * 3 + buffer(w * 3 - 1))) / 4);
            MapCtxs[j++] = clip((-NNEE + 3 * NE + clip(W * 4 - NW * 6 + NNW * 4 - buffer(w * 3 + 1))) / 3);
            MapCtxs[j++] = ((W + N) * 3 - NW * 2) / 4;
            for( j = 0; j < nOLS; j++ ) {
              ols[j].update(W);
              pOLS[j] = clip(int(floor(ols[j].predict(ols_ctxs[j]))));
            }

            cm.set(0);
            cm.set(hash(++i, N, px));
            cm.set(hash(++i, N - px));
            cm.set(hash(++i, W, px));
            cm.set(hash(++i, NW, px));
            cm.set(hash(++i, NE, px));
            cm.set(hash(++i, N, NN, px));
            cm.set(hash(++i, W, WW, px));
            cm.set(hash(++i, NE, NNEE, px));
            cm.set(hash(++i, NW, NNWW, px));
            cm.set(hash(++i, W, NEE, px));
            cm.set(hash(++i, (clamp4(W + N - NW, W, NW, N, NE) - px) / 2, logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW))));
            cm.set(hash(++i, (W - px) / 4, (NE - px) / 4, column[0]));
            cm.set(hash(++i, (clip(W * 2 - WW) - px) / 4, (clip(N * 2 - NN) - px) / 4));
            cm.set(hash(++i, (clamp4(N + NE - NNE, W, N, NE, NEE) - px) / 4, column[0]));
            cm.set(hash(++i, (clamp4(N + NW - NNW, W, NW, N, NE) - px) / 4, column[0]));
            cm.set(hash(++i, (W + NEE) / 4, px, column[0]));
            cm.set(hash(++i, clip(W + N - NW) - px, column[0]));
            cm.set(hash(++i, clamp4(N * 3 - NN * 3 + NNN, W, N, NN, NE), px, logMeanDiffQt(W, clip(NW * 2 - NNW))));
            cm.set(hash(++i, clamp4(W * 3 - WW * 3 + WWW, W, N, NE, NEE), px, logMeanDiffQt(N, clip(NW * 2 - NWW))));
            cm.set(hash(++i, (W + clamp4(NE * 3 - NNE * 3 + NNNE, W, N, NE, NEE)) / 2, px, logMeanDiffQt(N, (NW + NE) / 2)));
            cm.set(hash(++i, (N + NNN) / 8, clip(N * 3 - NN * 3 + NNN) / 4, px));
            cm.set(hash(++i, (W + WWW) / 8, clip(W * 3 - WW * 3 + WWW) / 4, px));
            cm.set(hash(++i, clip((-buffer(4) + 5 * WWW - 10 * WW + 10 * W +
                                   clamp4(NE * 4 - NNE * 6 + buffer(w * 3 - 1) * 4 - buffer(w * 4 - 1), N, NE, buffer(w - 2),
                                          buffer(w - 3))) / 5) - px));
            cm.set(hash(++i, clip(N * 2 - NN) - px, logMeanDiffQt(N, clip(NN * 2 - NNN))));
            cm.set(hash(++i, clip(W * 2 - WW) - px, logMeanDiffQt(NE, clip(N * 2 - NW))));

            if( isPNG )
              ctx = ((abs(W - N) > 8) << 10) | ((W > N) << 9) | ((abs(N - NW) > 8) << 8) | ((N > NW) << 7) | ((abs(N - NE) > 8) << 6) |
                    ((N > NE) << 5) | ((W > WW) << 4) | ((N > NN) << 3) | min(5, filterOn ? filter + 1 : 0);
            else
              ctx = min(0x1F, x / max(1, w / min(32, columns[0]))) | ((((abs(W - N) * 16 > W + N) << 1) | (abs(N - NW) > 8)) << 5) |
                    ((W + N) & 0x180);

            res = clamp4(W + N - NW, W, NW, N, NE) - px;
          }

          stats->Image.pixels.W = W;
          stats->Image.pixels.N = N;
          stats->Image.pixels.NN = NN;
          stats->Image.pixels.WW = WW;
          stats->Image.ctx = ctx >> gray;
        }
      }
      INJECT_SHARED_c0
      uint8_t B = (c0 << (8 - bpos));
      if( x || !isPNG ) {
        if( gray ) {
          int i = 0;
          map[i++].set_direct(0);
          map[i++].set_direct(
                  (((uint8_t) (clip(W + N - NW) - px - B)) * 8 + bpos) | (logMeanDiffQt(clip(N + NE - NNE), clip(N + NW - NNW)) << 11));

          for( int j = 0; j < nSM1; i++, j++ )
            map[i].set_direct((MapCtxs[j] - px - B) * 8 + bpos);

          for( int j = 0; i < nSM; i++, j++ )
            map[i].set_direct((pOLS[j] - px - B) * 8 + bpos);
        }
        sceneMap[2].set_direct(finalize64(hash(x, line), 19) * 8 + bpos);
        sceneMap[3].set_direct((prvFrmPx - B) * 8 + bpos);
        sceneMap[4].set_direct((prvFrmPrediction - B) * 8 + bpos);
      }

      // predict next bit
      if( x || !isPNG ) {
        cm.mix(m);
        if( gray ) {
          for( int i = 0; i < nSM; i++ ) {
            map[i].mix(m);
          }
        } else {
          for( int i = 0; i < nPltMaps; i++ ) {
            pltMap[i].set((bpos << 8) | iCtx[i]());
            pltMap[i].mix(m);
          }
        }
        for( int i = 0; i < 5; i++ ) {
          const int scale = (prevFramePos > 0 && prevFrameWidth == w) ? 64 : 0;
          sceneMap[i].setScale(scale);
          sceneMap[i].mix(m);
        }

        col = (col + 1) & 7;
        m.set(5 + ctx, 2048 + 5);
        m.set(col * 2 + (isPNG && c0 == ((0x100 | res) >> (8 - bpos))) + min(5, filterOn ? filter + 1 : 0) * 16, 6 * 16);
        m.set(((isPNG ? px : N + W) >> 4) + min(5, filterOn ? filter + 1 : 0) * 32, 6 * 32);
        m.set(c0, 256);
        m.set(((abs((int) (W - N)) > 4) << 9) | ((abs((int) (N - NE)) > 4) << 8) | ((abs((int) (W - NW)) > 4) << 7) | ((W > N) << 6) |
              ((N > NE) << 5) | ((W > NW) << 4) | ((W > WW) << 3) | ((N > NN) << 2) | ((NW > NNWW) << 1) | (NE > NNEE), 1024);
        m.set(min(63, column[0]), 64);
        m.set(min(127, column[1]), 128);
        m.set(min(255, (x + line) / 32), 256);
      } else {
        m.add(-2048 + ((filter >> (7 - bpos)) & 1) * 4096);
        m.set(min(4, filter), MIXERINPUTS);
      }
    }
};

#endif //PAQ8PX_IMAGE8BITMODEL_HPP
