#ifndef PAQ8PX_RECORDMODEL_HPP
#define PAQ8PX_RECORDMODEL_HPP

inline uint8_t clip(int const px) {
  if( px > 255 )
    return 255;
  if( px < 0 )
    return 0;
  return px;
}

inline uint8_t clamp4(const int px, const uint8_t n1, const uint8_t n2, const uint8_t n3, const uint8_t n4) {
  int maximum = n1;
  if( maximum < n2 )
    maximum = n2;
  if( maximum < n3 )
    maximum = n3;
  if( maximum < n4 )
    maximum = n4;
  int minimum = n1;
  if( minimum > n2 )
    minimum = n2;
  if( minimum > n3 )
    minimum = n3;
  if( minimum > n4 )
    minimum = n4;
  if( px < minimum )
    return minimum;
  if( px > maximum )
    return maximum;
  return px;
}

inline uint8_t logMeanDiffQt(const uint8_t a, const uint8_t b, const uint8_t limit = 7) {
  if( a == b )
    return 0;
  uint8_t sign = a > b ? 8 : 0;
  return sign | min(limit, ilog2((a + b) / max(2, abs(a - b) * 2) + 1));
}

inline uint32_t logQt(const uint8_t px, const uint8_t bits) {
  return (uint32_t(0x100 | px)) >> max(0, (int) (ilog2(px) - bits));
}

struct dBASE {
    uint8_t version;
    uint32_t nRecords;
    uint16_t recordLength, headerLength;
    uint32_t start, end;
};

/**
 * Model 2-D data with fixed record length. Also order 1-2 models that include the distance to the last match.
 */
class RecordModel {
private:
    static constexpr int nCM = 3 + 3 + 3 + 16; //cm,cn,co,cp
    static constexpr int nSM = 6;
    static constexpr int nSSM = 4;
    static constexpr int nIM = 3;
    static constexpr int nIndCtxs = 5;

public:
    static constexpr int MIXERINPUTS =
            nCM * ContextMap::MIXERINPUTS + nSM * StationaryMap::MIXERINPUTS + nSSM * SmallStationaryContextMap::MIXERINPUTS +
            nIM * IndirectMap::MIXERINPUTS; // 149
    static constexpr int MIXERCONTEXTS = 1024 + 512 + 11 * 32; //1888
    static constexpr int MIXERCONTEXTSETS = 3;

private:
    const Shared *const shared;
    ModelStats *stats;
    ContextMap cm, cn, co;
    ContextMap cp;
    StationaryMap maps[nSM];
    SmallStationaryContextMap sMap[nSSM];
    IndirectMap iMap[nIM];
    IndirectContext<uint16_t> iCtx[nIndCtxs];
    Array<uint32_t> cPos1 {256}, cpos2 {256}, cpos3 {256}, cpos4 {256};
    Array<uint32_t> wpos1 {256 * 256}; // buf(1..2) -> last position
    uint32_t rlen[3] = {2, 0, 0}; // run length and 2 candidates
    uint32_t rcount[2] = {0, 0}; // candidate counts
    uint8_t padding = 0; // detected padding byte
    uint8_t N = 0, NN = 0, NNN = 0, NNNN = 0, WxNW = 0;
    uint32_t prevTransition = 0, nTransition = 0; // position of the last padding transition
    uint32_t col = 0, mxCtx = 0, x = 0;
    bool mayBeImg24B = false;
    dBASE dbase {};

public:
    RecordModel(const Shared *const sh, ModelStats *st, const uint64_t size) : shared(sh), stats(st), cm(sh, 32768, 3),
            cn(sh, 32768 / 2, 3), co(sh, 32768 * 2, 3), cp(sh, size, 16), maps {{sh, 10, 8, 86, 1023},
                                                                                {sh, 10, 8, 86, 1023},
                                                                                {sh, 8,  8, 86, 1023},
                                                                                {sh, 8,  8, 86, 1023},
                                                                                {sh, 8,  8, 86, 1023},
                                                                                {sh, 11, 1, 86, 1023}}, sMap {{sh, 11, 1, 6, 86},
                                                                                                              {sh, 3,  1, 6, 86},
                                                                                                              {sh, 19, 1, 5, 128},
                                                                                                              {sh, 8,  8, 5, 64} // pos&255
            }, iMap {{sh, 8, 8, 86, 255},
                     {sh, 8, 8, 86, 255},
                     {sh, 8, 8, 86, 255}}, iCtx {{16, 8},
                                                 {16, 8},
                                                 {16, 8},
                                                 {20, 8},
                                                 {11, 1}} {}

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      // find record length
      if( bpos == 0 ) {
        INJECT_SHARED_c4
        INJECT_STATS_blpos
        INJECT_SHARED_buf
        INJECT_SHARED_pos
        int w = c4 & 0xffff, c = w & 255, d = w >> 8;
        if((stats->Wav) > 2 && (stats->Wav) != rlen[0] ) {
          rlen[0] = stats->Wav;
          rcount[0] = rcount[1] = 0;
        } else {
          // detect dBASE tables
          if( blpos == 0 || (dbase.version > 0 && blpos >= dbase.end))
            dbase.version = 0;
          else if( dbase.version == 0 && stats->blockType == DEFAULT && blpos >= 31 ) {
            uint8_t b = buf(32);
            if(((b & 7) == 3 || (b & 7) == 4 || (b >> 4) == 3 || b == 0xF5) && ((b = buf(30)) > 0 && b < 13) &&
               ((b = buf(29)) > 0 && b < 32) &&
               ((dbase.nRecords = buf(28) | (buf(27) << 8) | (buf(26) << 16) | (buf(25) << 24)) > 0 && dbase.nRecords < 0xFFFFF) &&
               ((dbase.headerLength = buf(24) | (buf(23) << 8)) > 32 && (((dbase.headerLength - 32 - 1) % 32) == 0 ||
                                                                         (dbase.headerLength > 255 + 8 &&
                                                                          (((dbase.headerLength -= 255 + 8) - 32 - 1) % 32) == 0))) &&
               ((dbase.recordLength = buf(22) | (buf(21) << 8)) > 8) && (buf(20) == 0 && buf(19) == 0 && buf(17) <= 1 && buf(16) <= 1)) {
              dbase.version = (((b = buf(32)) >> 4) == 3) ? 3 : b & 7;
              dbase.start = blpos - 32 + dbase.headerLength;
              dbase.end = dbase.start + dbase.nRecords * dbase.recordLength;
              if( dbase.version == 3 ) {
                rlen[0] = 32;
                rcount[0] = rcount[1] = 0;
              }
            }
          } else if( dbase.version > 0 && blpos == dbase.start ) {
            rlen[0] = dbase.recordLength;
            rcount[0] = rcount[1] = 0;
          }

          uint32_t r = pos - cPos1[c];
          if( r > 1 && r == cPos1[c] - cpos2[c] && r == cpos2[c] - cpos3[c] && (r > 32 || r == cpos3[c] - cpos4[c]) &&
              (r > 10 || ((c == buf(r * 5 + 1)) && c == buf(r * 6 + 1)))) {
            if( r == rlen[1] )
              ++rcount[0];
            else if( r == rlen[2] )
              ++rcount[1];
            else if( rcount[0] > rcount[1] )
              rlen[2] = r, rcount[1] = 1;
            else
              rlen[1] = r, rcount[0] = 1;
          }

          // check candidate lengths
          for( int i = 0; i < 2; i++ ) {
            if((int) rcount[i] > max(0, 12 - (int) ilog2(rlen[i + 1]))) {
              if( rlen[0] != rlen[i + 1] ) {
                if( mayBeImg24B && rlen[i + 1] == 3 ) {
                  rcount[0] >>= 1;
                  rcount[1] >>= 1;
                  continue;
                } else if((rlen[i + 1] > rlen[0]) && (rlen[i + 1] % rlen[0] == 0)) {
                  // maybe we found a multiple of the real record size..?
                  // in that case, it is probably an immediate multiple (2x).
                  // that is probably more likely the bigger the length, so
                  // check for small lengths too
                  if((rlen[0] > 32) && (rlen[i + 1] == rlen[0] * 2)) {
                    rcount[0] >>= 1;
                    rcount[1] >>= 1;
                    continue;
                  }
                }
                rlen[0] = rlen[i + 1];
                //printf("\nrecordModel: detected rlen: %d\n",rlen[0]); // for debugging
                rcount[i] = 0;
                mayBeImg24B = (rlen[0] > 30 && (rlen[0] % 3) == 0);
                nTransition = 0;
              } else
                // we found the same length again, that's positive reinforcement that
                // this really is the correct record size, so give it a little boost
                rcount[i] >>= 2;

              // if the other candidate record length is orders of
              // magnitude larger, it will probably never have enough time
              // to increase its counter before it's reset again. and if
              // this length is not a multiple of the other, than it might
              // really be worthwhile to investigate it, so we won't set its
              // counter to 0
              if( rlen[i + 1] << 4 > rlen[1 + (i ^ 1)] )
                rcount[i ^ 1] = 0;
            }
          }
        }

        assert(rlen[0] >= 2);
        col = pos % rlen[0];
        x = min(0x1F, col / max(1, rlen[0] / 32));
        N = buf(rlen[0]), NN = buf(rlen[0] * 2), NNN = buf(rlen[0] * 3), NNNN = buf(rlen[0] * 4);
        for( int i = 0; i < nIndCtxs - 1; iCtx[i] += c, i++ );
        iCtx[0] = (c << 8) | N;
        iCtx[1] = (buf(rlen[0] - 1) << 8) | N;
        iCtx[2] = (c << 8) | buf(rlen[0] - 1);
        iCtx[3] = finalize64(hash(c, N, buf(rlen[0] + 1)), 20);

        /*
        Consider record structures that include fixed-length strings.
        These usually contain the text followed by either spaces or 0's,
        depending on whether they're to be trimmed or they're null-terminated.
        That means we can guess the length of the string field by looking
        for small repetitions of one of these padding bytes followed by a
        different byte. By storing the last position where this transition
        occurred, and what was the padding byte, we are able to model these
        runs of padding bytes.
        Special care is taken to skip record structures of less than 9 bytes,
        since those may be little-endian 64 bit integers. If they contain
        relatively low values (<2^40), we may consistently get runs of 3 or
        even more 0's at the end of each record, and so we could assume that
        to be the general case. But with integers, we can't be reasonably sure
        that a number won't have 3 or more 0's just before a final non-zero MSB.
        And with such simple structures, there's probably no need to be fancy
        anyway
        */

        if( !col )
          nTransition = 0;
        if((((c4 >> 8) == SPACE * 0x010101) && (c != SPACE)) ||
           (!(c4 >> 8) && c && ((padding != SPACE) || (pos - prevTransition > rlen[0])))) {
          prevTransition = pos;
          nTransition += (nTransition < 31);
          padding = (uint8_t) d;
        }

        uint64_t i = 0;

        // Set 2-3 dimensional contexts
        // assuming rlen[0]<1024; col<4096
        cm.set(hash(++i, c << 8 | (min(255, pos - cPos1[c]) >> 2)));
        cm.set(hash(++i, w << 9 | llog(pos - wpos1[w]) >> 2));
        cm.set(hash(++i, rlen[0] | N << 10 | NN << 18));

        cn.set(hash(++i, w | rlen[0] << 16));
        cn.set(hash(++i, d | rlen[0] << 8));
        cn.set(hash(++i, c | rlen[0] << 8));

        co.set(hash(++i, c << 8 | min(255, pos - cPos1[c])));
        co.set(hash(++i, c << 17 | d << 9 | llog(pos - wpos1[w]) >> 2));
        co.set(hash(++i, c << 8 | N));

        cp.set(hash(++i, rlen[0] | N << 10 | col << 18));
        cp.set(hash(++i, rlen[0] | c << 10 | col << 18));
        cp.set(hash(++i, col | rlen[0] << 12));

        if( rlen[0] > 8 ) {
          cp.set(hash(++i, min(min(0xFF, rlen[0]), pos - prevTransition), min(0x3FF, col), (w & 0xF0F0) | (w == ((padding << 8) | padding)),
                      nTransition));
          cp.set(hash(++i, w, (buf(rlen[0] + 1) == padding && N == padding), col / max(1, rlen[0] / 32)));
        } else
          cp.set(0), cp.set(0);

        cp.set(hash(++i, N | ((NN & 0xF0) << 4) | ((NNN & 0xE0) << 7) | ((NNNN & 0xE0) << 10) | ((col / max(1, rlen[0] / 16)) << 18)));
        cp.set(hash(++i, (N & 0xF8) | ((NN & 0xF8) << 8) | (col << 16)));
        cp.set(hash(++i, N, NN));

        cp.set(hash(++i, col, iCtx[0]()));
        cp.set(hash(++i, col, iCtx[1]()));
        cp.set(hash(++i, col, iCtx[0]() & 0xFF, iCtx[1]() & 0xFF));

        cp.set(hash(++i, iCtx[2]()));
        cp.set(hash(++i, iCtx[3]()));
        cp.set(hash(++i, iCtx[1]() & 0xFF, iCtx[3]() & 0xFF));

        cp.set(hash(++i, N, (WxNW = c ^ buf(rlen[0] + 1))));
        cp.set(hash(++i, (stats->Match.length3 != 0) << 8 | stats->Match.expectedByte, uint8_t(iCtx[1]()), N, WxNW));

        int k = 0x300;
        if( mayBeImg24B ) {
          k = (col % 3) << 8;
          maps[0].set_direct(clip(((uint8_t) (c4 >> 16)) + c - (c4 >> 24)) | k);
        } else
          maps[0].set_direct(clip(c * 2 - d) | k);
        maps[1].set_direct(clip(c + N - buf(rlen[0] + 1)) | k);
        maps[2].set_direct(clip(N + NN - NNN));
        maps[3].set_direct(clip(N * 2 - NN));
        maps[4].set_direct(clip(N * 3 - NN * 3 + NNN));
        iMap[0].setDirect(N + NN - NNN);
        iMap[1].setDirect(N * 2 - NN);
        iMap[2].setDirect(N * 3 - NN * 3 + NNN);

        sMap[3].set(pos & 255); // mozilla

        // update last context positions
        cpos4[c] = cpos3[c];
        cpos3[c] = cpos2[c];
        cpos2[c] = cPos1[c];
        cPos1[c] = pos;
        wpos1[w] = pos;

        mxCtx = (rlen[0] > 128) ? (min(0x7F, col / max(1, rlen[0] / 128))) : col;
      }
      INJECT_SHARED_c0
      uint8_t B = c0 << (8 - bpos);
      uint32_t ctx = (N ^ B) | (bpos << 8);
      INJECT_SHARED_y
      iCtx[nIndCtxs - 1] += y;
      iCtx[nIndCtxs - 1] = ctx;
      maps[5].set_direct(ctx);

      sMap[0].set(ctx);
      sMap[1].set(iCtx[nIndCtxs - 1]());
      sMap[2].set((ctx << 8) | WxNW);

      cm.mix(m);
      cn.mix(m);
      co.mix(m);
      cp.mix(m);
      for( int i = 0; i < nSM; i++ )
        maps[i].mix(m);
      for( int i = 0; i < nIM; i++ )
        iMap[i].mix(m);
      for( int i = 0; i < nSSM; i++ )
        sMap[i].mix(m);

      m.set((rlen[0] > 2) * ((bpos << 7) | mxCtx), 1024);
      m.set(((N ^ B) >> 4) | (x << 4), 512);
      m.set(((stats->Text.chargrp) << 5) | x, 11 * 32);

      stats->Wav = min(0xFFFF, rlen[0]);
    }
};

#endif //PAQ8PX_RECORDMODEL_HPP
