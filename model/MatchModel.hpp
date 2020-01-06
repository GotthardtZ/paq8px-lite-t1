#ifndef PAQ8PX_MATCHMODEL_HPP
#define PAQ8PX_MATCHMODEL_HPP

/**
 * Predict the next bit based on a preceding long matching byte sequence
 *
 * This model monitors byte sequences and keeps their most recent positions in a hashtable.
 * When the current byte sequence matches an older sequence (having the same hash) the model predicts the forthcoming bits.
 */
class MatchModel {
private:
    static constexpr int NumHashes = 3;
    static constexpr int nCM = 2;
    static constexpr int nST = 3;
    static constexpr int nSSM = 2;
    static constexpr int nSM = 2;

public:
    static constexpr int MIXERINPUTS = 2 + nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS) + nST +
                                       nSSM * SmallStationaryContextMap::MIXERINPUTS + nSM * StationaryMap::MIXERINPUTS; // 23
    static constexpr int MIXERCONTEXTS = 8;
    static constexpr int MIXERCONTEXTSETS = 1;

private:
    const Shared *const shared;
    ModelStats *stats;
    enum Parameters : uint32_t {
        MaxExtend = 0, // longest allowed match expansion // warning: larger value -> slowdown
        MinLen = 5, // minimum required match length
        StepSize = 2, // additional minimum length increase per higher order hash
    };
    Array<uint32_t> Table;
    StateMap StateMaps[nST];
    ContextMap2 cm;
    SmallStationaryContextMap SCM;
    StationaryMap Maps[nSM];
    IndirectContext<uint8_t> iCtx;
    uint32_t hashes[NumHashes] {0};
    uint32_t ctx[nST] {0};
    uint32_t length = 0; // rebased length of match (length=1 represents the smallest accepted match length), or 0 if no match
    uint32_t index = 0; // points to next byte of match in buf, 0 when there is no match
    uint32_t length_bak = 0; // allows match recovery after a 1-byte mismatch
    uint32_t index_bak = 0; //
    uint8_t expectedByte = 0; // prediction is based on this byte (buf[index]), valid only when length>0
    bool delta = false; // indicates that a match has just failed (delta mode)
    const uint32_t mask;
    const int hashBits;

public:
    MatchModel(const Shared *const sh, ModelStats *st, const uint64_t Size) : shared(sh), stats(st), Table(Size / sizeof(uint32_t)),
            StateMaps {//StateMap:  s, n, lim,
                    {sh, 1, 56 * 256,          1023, StateMap::GENERIC},
                    {sh, 1, 8 * 256 * 256 + 1, 1023, StateMap::GENERIC},
                    {sh, 1, 256 * 256,         1023, StateMap::GENERIC}}, cm(sh, MEM / 32, nCM, 74, CM_USE_RUN_STATS),
            SCM {sh, 6, 1, 6, 64}, /* SmallStationaryContextMap: BitsOfContext, InputBits, Rate, Scale */
            Maps {/* StationaryMap: BitsOfContext, InputBits, Scale, Limit  */
                    {sh, 23, 1, 64, 1023},
                    {sh, 15, 1, 64, 1023}}, iCtx {15, 1}, // IndirectContext<uint8_t>: BitsPerContext, InputBits
            mask(uint32_t(Size / sizeof(uint32_t) - 1)), hashBits(ilog2(mask + 1)) {
      assert(isPowerOf2(Size));
    }

    void update() {
      INJECT_SHARED_bpos
      INJECT_SHARED_buf
      if( length != 0 ) {
        const int expectedBit = (expectedByte >> ((8 - bpos) & 7)) & 1;
        INJECT_SHARED_y
        if( y != expectedBit ) {
          if( length_bak != 0 && length - length_bak < MinLen ) { // mismatch too soon in recovery mode -> give up
            length_bak = 0;
            index_bak = 0;
          } else { //backup match information: maybe we can recover it just after this mismatch
            length_bak = length;
            index_bak = index;
          }
          delta = true;
          length = 0;
        }
      }

      //bytewise contexts
      if( bpos == 0 ) {
        // update hashes
        for( uint32_t i = 0, minLen = MinLen + (NumHashes - 1) * StepSize; i < NumHashes; i++, minLen -= StepSize ) {
          uint64_t hash = 0;
          for( uint32_t j = minLen; j > 0; j-- )
            hash = combine64(hash, buf(j));
          hashes[i] = finalize64(hash, hashBits);
        }

        // recover match after a 1-byte mismatch
        if( length == 0 && !delta && length_bak != 0 ) { //match failed (2 bytes ago), no new match found, and we have a backup
          index_bak++;
          if( length_bak < mask )
            length_bak++;
          INJECT_SHARED_c1
          if( buf[index_bak] == c1 ) { // match continues -> recover
            length = length_bak;
            index = index_bak;
          } else { // still mismatch
            length_bak = index_bak = 0; // purge backup
          }
        }
        // extend current match
        if( length != 0 ) {
          index++;
          if( length < mask )
            length++;
        }
        delta = false;

        // find a new match, starting with the highest order hash and falling back to lower ones
        if( length == 0 ) {
          uint32_t minLen = MinLen + (NumHashes - 1) * StepSize, bestLen = 0, bestIndex = 0;
          for( uint32_t i = 0; i < NumHashes && length < minLen; i++, minLen -= StepSize ) {
            index = Table[hashes[i]];
            if( index > 0 ) {
              length = 0;
              while( length < (minLen + MaxExtend) && buf(length + 1) == buf[index - length - 1] )
                length++;
              if( length > bestLen ) {
                bestLen = length;
                bestIndex = index;
              }
            }
          }
          if( bestLen >= MinLen ) {
            length = bestLen - (MinLen - 1); // rebase, a length of 1 actually means a length of MinLen
            index = bestIndex;
            length_bak = index_bak = 0; // purge any backup
          } else
            length = index = 0;
        }
        // update position information in hashtable
        INJECT_SHARED_pos
        for( uint32_t i = 0; i < NumHashes; i++ )
          Table[hashes[i]] = pos;
        stats->Match.expectedByte = expectedByte = (length != 0 ? buf[index] : 0);
      }
    }

    void mix(Mixer &m) {
      update();

      for( uint32_t i = 0; i < nST; i++ ) // reset contexts
        ctx[i] = 0;

      INJECT_SHARED_bpos
      INJECT_SHARED_c1
      INJECT_SHARED_c0
      const int expectedBit = length != 0 ? (expectedByte >> (7 - bpos)) & 1 : 0;
      if( length != 0 ) {
        if( length <= 16 )
          ctx[0] = (length - 1) * 2 + expectedBit; // 0..31
        else
          ctx[0] = 24 + (min(length - 1, 63) >> 2) * 2 + expectedBit; // 32..55
        ctx[0] = ((ctx[0] << 8) | c0);
        ctx[1] = ((expectedByte << 11) | (bpos << 8) | c1) + 1;
        const int sign = 2 * expectedBit - 1;
        m.add(sign * (min(length, 32) << 5)); // +/- 32..1024
        m.add(sign * (ilog(min(length, 65535)) << 2)); // +/-  0..1024
      } else { // no match at all or delta mode
        m.add(0);
        m.add(0);
      }

      if( delta ) // delta mode: helps predicting the remaining bits of a character when a mismatch occurs
        ctx[2] = (expectedByte << 8) | c0;

      for( uint32_t i = 0; i < nST; i++ ) {
        const uint32_t c = ctx[i];
        if( c != 0 )
          m.add(stretch(StateMaps[i].p1(c)) >> 1);
        else
          m.add(0);
      }

      const uint32_t length_ilog2 = ilog2(length + 1);
      //no match:      length_ilog2=0
      //length=1..2:   length_ilog2=1
      //length=3..6:   length_ilog2=2
      //length=7..14:  length_ilog2=3
      //length=15..30: length_ilog2=4

      const uint8_t length3 = min(length_ilog2, 3); // 2 bits
      const uint8_t rm = length_bak != 0 && length - length_bak == 1; // predicting the first byte in recovery mode is still uncertain
      const uint8_t length3Rm = length3 << 1 | rm; // 3 bits

      //bytewise contexts
      INJECT_SHARED_c4
      if( bpos == 0 ) {
        if( length != 0 ) {
          cm.set(hash(0, expectedByte, length3Rm));
          cm.set(hash(1, expectedByte, length3Rm, c1));
        } else {
          // when there is no match it is still slightly beneficial not to skip(), but set some low-order contexts
          cm.set(hash(2, c4 & 0xff)); // order 1
          cm.set(hash(3, c4 & 0xffff)); // order 2
          //cm.skip();
          //cm.skip();
        }
      }
      cm.mix(m);

      //bitwise contexts
      {
        Maps[0].set(hash(expectedByte, c0, c4 & 0xffff, length3Rm));
        INJECT_SHARED_y
        iCtx += y;
        const uint8_t C = length3Rm << 1 | expectedBit; // 4 bits
        iCtx = (bpos << 11) | (c1 << 3) | C;
        Maps[1].set_direct(iCtx());
        SCM.set((bpos << 3) | C);
      }
      Maps[0].mix(m);
      Maps[1].mix(m);
      SCM.mix(m);

      const uint32_t length_C = length_ilog2 != 0 ? length_ilog2 + 1 : delta;
      //no match, no delta mode:   length_C=0
      //failed match, delta mode:  length_C=1
      //length=1..2:   length_C=2
      //length=3..6:   length_C=3
      //length=7..14:  length_C=4
      //length=15..30: length_C=5

      m.set(min(length_C, 7), 8);
      stats->Match.length3 = min(length_C, 3);
    }
};

#endif //PAQ8PX_MATCHMODEL_HPP
