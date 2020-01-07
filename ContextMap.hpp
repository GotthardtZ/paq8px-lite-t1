#ifndef PAQ8PX_CONTEXTMAP_HPP
#define PAQ8PX_CONTEXTMAP_HPP

/////////////////////////// ContextMap /////////////////////////
// TODO: update this documentation
// A ContextMap maps contexts to bit histories and makes predictions
// to a Mixer.  Methods common to all classes:
//
// ContextMap cm(m, c); creates using about m bytes of memory (a power
//   of 2) for c contexts.
// cm.set(cx);  sets the next context to cx, called up to c times
//   cx is an arbitrary 32 bit value that identifies the context.
//   It should be called before predicting the first bit of each byte.
// cm.mix(m) updates Mixer m with the next prediction.  Returns 1
//   if context cx is found, else 0.  Then it extends all the contexts with
//   global bit y.  It should be called for every bit:
//
//     if (bitPosition==0)
//       for (int i=0; i<c; ++i) cm.set(cxt[i]);
//     cm.mix(m);
//
// The different types are as follows:
//

// - SmallStationaryContextMap.  0 <= cx*256 < m.
//     The state is a 16-bit probability that is adjusted after each
//     prediction.  c=1.
// - ContextMap.  For large contexts, c >= 1.  context must be hashed.

// context map for large contexts.  Most modeling uses this type of context
// map.  It includes a built in RunContextMap to predict the last byte seen
// in the same context, and also bit-level contexts that map to a bit
// history state.
//
// Bit histories are stored in a hash table.  The table is organized into
// 64-byte buckets aligned on cache page boundaries.  Each bucket contains
// a hash chain of 7 elements, plus a 2 element queue (packed into 1 byte)
// of the last 2 elements accessed for LRU replacement.  Each element has
// a 2 byte checksum for detecting collisions, and an array of 7 bit history
// states indexed by the last 0 to 2 bits of context.  The buckets are indexed
// by a context ending after 0, 2, or 5 bits of the current byte.  Thus, each
// byte modeled results in 3 main memory accesses per context, with all other
// accesses to cache.
//
// On bits 0, 2 and 5, the context is updated and a new bucket is selected.
// The most recently accessed element is tried first, by comparing the
// 16 bit checksum, then the 7 elements are searched linearly.  If no match
// is found, then the element with the lowest priority among the 5 elements
// not in the LRU queue is replaced.  After a replacement, the queue is
// emptied (so that consecutive misses favor a LFU replacement policy).
// In all cases, the found/replaced element is put in the front of the queue.
//
// The priority is the state number of the first element (the one with 0
// additional bits of context).  The states are sorted by increasing n0+n1
// (number of bits seen), implementing a LFU replacement policy.
//
// When the context ends on a byte boundary (bit 0), only 3 of the 7 bit
// history states are used.  The remaining 4 bytes implement a run model
// as follows: <count:7,d:1> <b1> <unused> <unused> where <b1> is the last byte
// seen, possibly repeated.  <count:7,d:1> is a 7 bit count and a 1 bit
// flag (represented by count * 2 + d).  If d=0 then <count> = 1..127 is the
// number of repeats of <b1> and no other bytes have been seen.  If d is 1 then
// other byte values have been seen in this context prior to the last <count>
// copies of <b1>.
//
// As an optimization, the last two hash elements of each byte (representing
// contexts with 2-7 bits) are not updated until a context is seen for
// a second time.  This is indicated by <count,d> = <1,0> (2).  After update,
// <count,d> is updated to <2,0> or <1,1> (4 or 3).

class ContextMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 5;

private:
    const Shared *const shared;
    Random rnd;
    const int c; // max number of contexts
    class E { // hash element, 64 bytes
        uint16_t chk[7]; // byte context checksums
        uint8_t last; // last 2 accesses (0-6) in low, high nibble
    public:
        uint8_t bh[7][7]; // byte context, 3-bit context -> bit history state
        // bh[][0] = 1st bit, bh[][1,2] = 2nd bit, bh[][3..6] = 3rd bit
        // bh[][0] is also a replacement priority, 0 = empty
        inline uint8_t *get(const uint16_t checksum) { // find element (0-6) matching checksum.
          // If not found, insert or replace lowest priority (not last).
          // find or create hash element matching checksum ch
          if( chk[last & 15U] == checksum )
            return &bh[last & 15U][0];
          int b = 0xffff, bi = 0;
          for( int i = 0; i < 7; ++i ) {
            if( chk[i] == checksum ) {
              last = last << 4U | i;
              return &bh[i][0];
            }
            int pri = bh[i][0];
            if( pri < b && (last & 15U) != i && last >> 4U != i ) {
              b = pri;
              bi = i;
            }
          }
          return last = 0xf0U | bi, chk[bi] = checksum, (uint8_t *) memset(&bh[bi][0], 0, 7);
        }
    };

    Array<E, 64> t; // bit histories for bits 0-1, 2-4, 5-7
    // For 0-1, also contains a run count in bh[][4] and value in bh[][5]
    // and pending update count in bh[7]
    Array<uint8_t *> cp; // c pointers to current bit history
    Array<uint8_t *> cp0; // First element of 7 element array containing cp[i]
    Array<uint32_t> cxt; // c whole byte context hashes
    Array<uint16_t> chk; // c whole byte context checksums
    Array<uint8_t *> runP; // c [0..3] = count, value, unused, unused
    StateMap sm; // c maps of state -> p
    int cn; // next context to set by set()
    const uint32_t mask;
    const int hashBits;
    uint64_t validFlags;

public:
    // Construct using m bytes of memory for c contexts
    ContextMap(const Shared *const sh, uint64_t m, const int c) : shared(sh), c(c), t(m >> 6), cp(c), cp0(c), cxt(c), chk(c), runP(c),
            sm(sh, c, 256, 1023, StateMap::BitHistory), cn(0), mask(uint32_t(t.size() - 1)), hashBits(ilog2(mask + 1)), validFlags(0) {
      assert(m >= 64 && isPowerOf2(m));
      assert(sizeof(E) == 64);
      assert(c <= (int) sizeof(validFlags) * 8); // validFlags is 64 bits - it can't support more than 64 contexts
    }

    // set next whole byte context to cx
    inline void set(const uint64_t cx) {
      assert(cn >= 0 && cn < c);
      const uint32_t ctx = cxt[cn] = finalize64(cx, hashBits);
      const uint16_t checksum = chk[cn] = (uint16_t) checksum64(cx, hashBits, 16);
      uint8_t *base = cp0[cn] = cp[cn] = t[ctx & mask].get(checksum);
      runP[cn] = base + 3;
      // update pending bit histories for bits 2-7
      if( base[3] == 2 ) {
        const int c = base[4] + 256;
        uint8_t *p = t[(ctx + (c >> 6)) & mask].get(checksum);
        p[0] = 1 + ((c >> 5) & 1);
        p[1 + ((c >> 5) & 1)] = 1 + ((c >> 4) & 1);
        p[3 + ((c >> 4) & 3)] = 1 + ((c >> 3) & 1);
        p = t[(ctx + (c >> 3)) & mask].get(checksum);
        p[0] = 1 + ((c >> 2) & 1U);
        p[1 + ((c >> 2U) & 1U)] = 1 + ((c >> 1) & 1U);
        p[3 + ((c >> 1U) & 3U)] = 1 + (c & 1U);
      }
      cn++;
      validFlags = (validFlags << 1) + 1;
    }

    inline void skip() {
      assert(cn >= 0 && cn < c);
      cn++;
      validFlags <<= 1U;
    }

    void update() override {
      INJECT_SHARED_bpos
      INJECT_SHARED_c0
      INJECT_SHARED_c1
      INJECT_SHARED_y
      for( int i = 0; i < cn; ++i ) {
        if(((validFlags >> (cn - 1 - i)) & 1) != 0 ) {
          // update bit history state byte
          if( cp[i] != nullptr ) {
            assert(cp[i] >= &t[0].bh[0][0] && cp[i] <= &t[t.size() - 1].bh[6][6]);
            assert((uintptr_t(cp[i]) & 63) >= 15);
            StateTable::update(cp[i], y, rnd);
          }

          // update context pointers
          if( bpos > 1 && runP[i][0] == 0 )
            cp[i] = nullptr;
          else {
            switch( bpos ) {
              case 1:
              case 3:
              case 6:
                cp[i] = cp0[i] + 1 + (c0 & 1U);
                break;
              case 4:
              case 7:
                cp[i] = cp0[i] + 3 + (c0 & 3U);
                break;
              case 2:
              case 5: {
                const uint16_t checksum = chk[i];
                const uint32_t ctx = cxt[i];
                cp0[i] = cp[i] = t[(ctx + c0) & mask].get(checksum);
                break;
              }
              case 0: {
                // update run count of previous context
                if( runP[i][0] == 0 ) // new context
                  runP[i][0] = 2, runP[i][1] = c1;
                else if( runP[i][1] != c1 ) // different byte in context
                  runP[i][0] = 1, runP[i][1] = c1;
                else if( runP[i][0] < 254 ) // same byte in context
                  runP[i][0] += 2;
                else if( runP[i][0] == 255 )
                  runP[i][0] = 128;
                break;
              }
            }
          }
        }
      }
      if( bpos == 0 ) {
        cn = 0;
        validFlags = 0;
      }
    }

    void mix(Mixer &m) {
      updater.subscribe(this);
      sm.subscribe();
      INJECT_SHARED_bpos
      INJECT_SHARED_c0
      for( int i = 0; i < cn; ++i ) {
        if(((validFlags >> (cn - 1 - i)) & 1) != 0 ) {
          // predict from last byte in context
          if((runP[i][1] + 256) >> (8 - bpos) == c0 ) {
            int rc = runP[i][0]; // count*2, +1 if 2 different bytes seen
            int sign = (runP[i][1] >> (7 - bpos) & 1) * 2 - 1; // predicted bit + for 1, - for 0
            int c = ilog(rc + 1) << (2 + (~rc & 1));
            m.add(sign * c);
          } else
            m.add(0); //p=0.5

          // predict from bit context
          const int s = cp[i] != nullptr ? *cp[i] : 0;
          if( s == 0 ) { //skip context
            sm.skip(i);
            m.add(0);
            m.add(0);
            m.add(0);
            m.add(0);
          } else {
            const int p1 = sm.p2(i, s);
            const int st = stretch(p1) >> 2;
            const int contextIsYoung = int(s <= 2);
            m.add(st >> contextIsYoung);
            m.add((p1 - 2048) >> 3U);
            const int n0 = -!StateTable::next(s, 2);
            const int n1 = -!StateTable::next(s, 3);
            m.add((n0 | n1) & st); // when both counts are nonzero add(0) otherwise add(st)
            const int p0 = 4095 - p1;
            m.add(((p1 & n0) - (p0 & n1)) >> 4);
          }
        } else { //skipped context
          sm.skip(i);
          m.add(0);
          m.add(0);
          m.add(0);
          m.add(0);
          m.add(0);
        }
      }
    }
};

#endif //PAQ8PX_CONTEXTMAP_HPP
