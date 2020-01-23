#include "ContextMap.hpp"

ContextMap::ContextMap(uint64_t m, const int c) : c(c), t(m >> 6U), cp(c), cp0(c), cxt(c), chk(c), runP(c),
        sm(c, 256, 1023, StateMap::BitHistory), cn(0), mask(uint32_t(t.size() - 1)), hashBits(ilog2(mask + 1)), validFlags(0) {
#ifndef NDEBUG
  printf("Created ContextMap with m = %llu, c = %d\n", m, c);
#endif
  assert(m >= 64 && isPowerOf2(m));
  static_assert(sizeof(Bucket) == 64, "Size of Bucket should be 64!");
  assert(c <= (int) sizeof(validFlags) * 8); // validFlags is 64 bits - it can't support more than 64 contexts
}

void ContextMap::set(const uint64_t cx) {
  assert(cn >= 0 && cn < c);
  const uint32_t ctx = cxt[cn] = finalize64(cx, hashBits);
  const uint16_t checksum = chk[cn] = static_cast<uint16_t>(checksum64(cx, hashBits, 16));
  uint8_t *base = cp0[cn] = cp[cn] = t[ctx & mask].find(checksum);
  runP[cn] = base + 3;
  // update pending bit histories for bits 2-7
  if( base[3] == 2 ) {
    const int c = base[4] + 256;
    uint8_t *p = t[(ctx + (c >> 6U)) & mask].find(checksum);
    p[0] = 1 + ((c >> 5U) & 1U);
    p[1 + ((c >> 5U) & 1U)] = 1 + ((c >> 4U) & 1U);
    p[3 + ((c >> 4U) & 3U)] = 1 + ((c >> 3U) & 1U);
    p = t[(ctx + (c >> 3U)) & mask].find(checksum);
    p[0] = 1 + ((c >> 2U) & 1U);
    p[1 + ((c >> 2U) & 1U)] = 1 + ((c >> 1U) & 1U);
    p[3 + ((c >> 1U) & 3U)] = 1 + (c & 1U);
  }
  cn++;
  validFlags = (validFlags << 1U) + 1;
}

void ContextMap::skip() {
  assert(cn >= 0 && cn < c);
  cn++;
  validFlags <<= 1U;
}

void ContextMap::update() {
  INJECT_SHARED_y
  for( int i = 0; i < cn; ++i ) {
    if(((validFlags >> (cn - 1 - i)) & 1) != 0 ) {
      // update bit history state byte
      if( cp[i] != nullptr ) {
        assert(cp[i] >= &t[0].bitState[0][0] && cp[i] <= &t[t.size() - 1].bitState[6][6]);
        assert((uintptr_t(cp[i]) & 63) >= 15);
        StateTable::update(cp[i], y, rnd);
      }

      // update context pointers
      if( shared->bitPosition > 1 && runP[i][0] == 0 ) {
        cp[i] = nullptr;
      } else {
        switch( shared->bitPosition ) {
          case 1:
          case 3:
          case 6:
            cp[i] = cp0[i] + 1 + (shared->c0 & 1U);
            break;
          case 4:
          case 7:
            cp[i] = cp0[i] + 3 + (shared->c0 & 3U);
            break;
          case 2:
          case 5: {
            const uint16_t checksum = chk[i];
            const uint32_t ctx = cxt[i];
            cp0[i] = cp[i] = t[(ctx + shared->c0) & mask].find(checksum);
            break;
          }
          case 0: {
            // update run count of previous context
            if( runP[i][0] == 0 ) { // new context
              runP[i][0] = 2, runP[i][1] = shared->c1;
            } else if( runP[i][1] != shared->c1 ) { // different byte in context
              runP[i][0] = 1, runP[i][1] = shared->c1;
            } else if( runP[i][0] < 254 ) { // same byte in context
              runP[i][0] += 2;
            } else if( runP[i][0] == 255 ) {
              runP[i][0] = 128;
            }
            break;
          }
          default:
            assert(false);
        }
      }
    }
  }
  if( shared->bitPosition == 0 ) {
    cn = 0;
    validFlags = 0;
  }
}

void ContextMap::mix(Mixer &m) {
  shared->updateBroadcaster->subscribe(this);
  sm.subscribe();
  for( int i = 0; i < cn; ++i ) {
    if(((validFlags >> (cn - 1 - i)) & 1U) != 0 ) {
      // predict from last byte in context
      if((runP[i][1] + 256) >> (8 - shared->bitPosition) == shared->c0 ) {
        int rc = runP[i][0]; // count*2, +1 if 2 different bytes seen
        int sign = (runP[i][1] >> (7 - shared->bitPosition) & 1U) * 2 - 1; // predicted bit + for 1, - for 0
        int c = ilog->log(rc + 1) << (2 + (~rc & 1U));
        m.add(sign * c);
      } else {
        m.add(0); //p=0.5
      }

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
        const int st = stretch(p1) >> 2U;
        const int contextIsYoung = int(s <= 2);
        m.add(st >> contextIsYoung);
        m.add((p1 - 2048) >> 3U);
        const int n0 = static_cast<const int>(-static_cast<int>(StateTable::next(s, 2)) == 0U);
        const int n1 = static_cast<const int>(-static_cast<int>(StateTable::next(s, 3)) == 0U);
        m.add((n0 | n1) & st); // when both counts are nonzero add(0) otherwise add(st)
        const int p0 = 4095 - p1;
        m.add(((p1 & n0) - (p0 & n1)) >> 4U);
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
