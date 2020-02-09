#include "MatchModel.hpp"

MatchModel::MatchModel(ModelStats *st, const uint64_t size) : stats(st), table(size / sizeof(uint32_t)),
        stateMaps {{1, 56 * 256,          1023, StateMap::Generic},
                   {1, 8 * 256 * 256 + 1, 1023, StateMap::Generic},
                   {1, 256 * 256,         1023, StateMap::Generic}}, cm(shared->mem / 32, nCM, 74, CM_USE_RUN_STATS), SCM {6, 1, 6, 64},
        maps {{23, 1, 64, 1023},
              {15, 1, 64, 1023}}, iCtx {15, 1}, mask(uint32_t(size / sizeof(uint32_t) - 1)), hashBits(ilog2(mask + 1)) {
#ifndef NDEBUG
  printf("Created MatchModel with size = %llu\n", size);
#endif
  assert(isPowerOf2(size));

}

void MatchModel::update() {
  INJECT_SHARED_buf
  if( length != 0 ) {
    const int expectedBit = (expectedByte >> ((8 - shared->bitPosition) & 7U)) & 1U;
    INJECT_SHARED_y
    if( y != expectedBit ) {
      if( lengthBak != 0 && length - lengthBak < MinLen ) { // mismatch too soon in recovery mode -> give up
        lengthBak = 0;
        indexBak = 0;
      } else { //backup match information: maybe we can recover it just after this mismatch
        lengthBak = length;
        indexBak = index;
      }
      delta = true;
      length = 0;
    }
  }

  //bytewise contexts
  if( shared->bitPosition == 0 ) {
    // update hashes
    for( uint32_t i = 0, minLen = MinLen + (numHashes - 1) * StepSize; i < numHashes; i++, minLen -= StepSize ) {
      uint64_t hash = 0;
      for( uint32_t j = minLen; j > 0; j-- ) {
        hash = combine64(hash, buf(j));
      }
      hashes[i] = finalize64(hash, hashBits);
    }

    // recover match after a 1-byte mismatch
    if( length == 0 && !delta && lengthBak != 0 ) { //match failed (2 bytes ago), no new match found, and we have a backup
      indexBak++;
      if( lengthBak < mask ) {
        lengthBak++;
      }
      if( buf[indexBak] == shared->c1 ) { // match continues -> recover
        length = lengthBak;
        index = indexBak;
      } else { // still mismatch
        lengthBak = indexBak = 0; // purge backup
      }
    }
    // extend current match
    if( length != 0 ) {
      index++;
      if( length < mask ) {
        length++;
      }
    }
    delta = false;

    // find a new match, starting with the highest order hash and falling back to lower ones
    if( length == 0 ) {
      uint32_t minLen = MinLen + (numHashes - 1) * StepSize;
      uint32_t bestLen = 0;
      uint32_t bestIndex = 0;
      for( uint32_t i = 0; i < numHashes && length < minLen; i++, minLen -= StepSize ) {
        index = table[hashes[i]];
        if( index > 0 ) {
          length = 0;
          while( length < (minLen + MaxExtend) && buf(length + 1) == buf[index - length - 1] ) {
            length++;
          }
          if( length > bestLen ) {
            bestLen = length;
            bestIndex = index;
          }
        }
      }
      if( bestLen >= MinLen ) {
        length = bestLen - (MinLen - 1); // rebase, a length of 1 actually means a length of MinLen
        index = bestIndex;
        lengthBak = indexBak = 0; // purge any backup
      } else {
        length = index = 0;
      }
    }
    // update position information in hashtable
    for( uint32_t i = 0; i < numHashes; i++ ) {
      table[hashes[i]] = shared->buf.getpos();
    }
    stats->Match.expectedByte = expectedByte = (length != 0 ? buf[index] : 0);
  }
}

void MatchModel::mix(Mixer &m) {
  update();

  for( uint32_t i = 0; i < nST; i++ ) { // reset contexts
    ctx[i] = 0;
  }

  const int expectedBit = length != 0 ? (expectedByte >> (7 - shared->bitPosition)) & 1U : 0;
  if( length != 0 ) {
    if( length <= 16 ) {
      ctx[0] = (length - 1) * 2 + expectedBit; // 0..31
    } else {
      ctx[0] = 24 + (min(length - 1, 63) >> 2U) * 2 + expectedBit; // 32..55
    }
    ctx[0] = ((ctx[0] << 8U) | shared->c0);
    ctx[1] = ((expectedByte << 11U) | (shared->bitPosition << 8U) | shared->c1) + 1;
    const int sign = 2 * expectedBit - 1;
    m.add(sign * (min(length, 32) << 5U)); // +/- 32..1024
    m.add(sign * (ilog->log(min(length, 65535)) << 2U)); // +/-  0..1024
  } else { // no match at all or delta mode
    m.add(0);
    m.add(0);
  }

  if( delta ) { // delta mode: helps predicting the remaining bits of a character when a mismatch occurs
    ctx[2] = (expectedByte << 8U) | shared->c0;
  }

  for( uint32_t i = 0; i < nST; i++ ) {
    const uint32_t c = ctx[i];
    if( c != 0 ) {
      m.add(stretch(stateMaps[i].p1(c)) >> 1U);
    } else {
      m.add(0);
    }
  }

  const uint32_t lengthIlog2 = ilog2(length + 1);
  //no match:      lengthIlog2=0
  //length=1..2:   lengthIlog2=1
  //length=3..6:   lengthIlog2=2
  //length=7..14:  lengthIlog2=3
  //length=15..30: lengthIlog2=4

  const uint8_t length3 = min(lengthIlog2, 3); // 2 bits
  const auto rm = static_cast<const uint8_t>(lengthBak != 0 &&
                                             length - lengthBak == 1); // predicting the first byte in recovery mode is still uncertain
  const uint8_t length3Rm = length3 << 1U | rm; // 3 bits

  //bytewise contexts
  if( shared->bitPosition == 0 ) {
    if( length != 0 ) {
      cm.set(hash(0, expectedByte, length3Rm));
      cm.set(hash(1, expectedByte, length3Rm, shared->c1));
    } else {
      // when there is no match it is still slightly beneficial not to skip(), but set some low-order contexts
      cm.set(hash(2, shared->c4 & 0xffu)); // order 1
      cm.set(hash(3, shared->c4 & 0xffffu)); // order 2
      //cm.skip();
      //cm.skip();
    }
  }
  cm.mix(m);

  //bitwise contexts
  {
    maps[0].set(hash(expectedByte, shared->c0, shared->c4 & 0xffffu, length3Rm));
    INJECT_SHARED_y
    iCtx += y;
    const uint8_t c = length3Rm << 1U | expectedBit; // 4 bits
    iCtx = (shared->bitPosition << 11U) | (shared->c1 << 3U) | c;
    maps[1].setDirect(iCtx());
    SCM.set((shared->bitPosition << 3U) | c);
  }
  maps[0].mix(m);
  maps[1].mix(m);
  SCM.mix(m);

  const uint32_t lengthC = lengthIlog2 != 0 ? lengthIlog2 + 1 : static_cast<unsigned int>(delta);
  //no match, no delta mode:   lengthC=0
  //failed match, delta mode:  lengthC=1
  //length=1..2:   lengthC=2
  //length=3..6:   lengthC=3
  //length=7..14:  lengthC=4
  //length=15..30: lengthC=5

  m.set(min(lengthC, 7), 8);
  stats->Match.length3 = min(lengthC, 3);
}
