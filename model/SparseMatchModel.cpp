#include "SparseMatchModel.hpp"

SparseMatchModel::SparseMatchModel(const uint64_t size) : table(size / sizeof(uint32_t)), maps {{22, 1, 128, 1023},
                                                                                                {17, 4, 128, 1023},
                                                                                                {8,  1, 128, 1023},
                                                                                                {19, 1, 128, 1023}},
        mask(uint32_t(size / sizeof(uint32_t) - 1)), hashBits(ilog2(mask + 1)) {
  assert(isPowerOf2(size));
}

void SparseMatchModel::update() {
  INJECT_SHARED_buf
  // update sparse hashes
  for( uint32_t i = 0; i < numHashes; i++ ) {
    uint64_t hash = 0;
    for( uint32_t j = 0, k = sparse[i].offset + 1; j < sparse[i].minLen; j++, k += sparse[i].stride ) {
      hash = combine64(hash, static_cast<uint8_t>(buf(k) & sparse[i].bitMask));
    }
    hashes[i] = finalize64(hash, hashBits);
  }
  // extend current match, if available
  if( length != 0u ) {
    index++;
    if( length < MaxLen ) {
      length++;
    }
  }
    // or find a new match
  else {
    for( int i = list.getFirst(); i >= 0; i = list.getNext()) {
      index = table[hashes[i]];
      if( index > 0 ) {
        uint32_t offset = sparse[i].offset + 1;
        while( length < sparse[i].minLen && ((buf(offset) ^ buf[index - offset]) & sparse[i].bitMask) == 0 ) {
          length++;
          offset += sparse[i].stride;
        }
        if( length >= sparse[i].minLen ) {
          length -= (sparse[i].minLen - 1);
          index += sparse[i].deletions;
          hashIndex = i;
          list.moveToFront(i);
          break;
        }
      }
      length = index = 0;
    }
  }
  // update position information in hashtable
  for( uint32_t i = 0; i < numHashes; i++ ) {
    table[hashes[i]] = shared->buf.getpos();
  }

  expectedByte = length != 0 ? buf[index] : 0;
  if( valid ) {
    INJECT_SHARED_y
    iCtx8 += y;
    iCtx16 += shared->c1;
  }
  valid = length > 1; // only predict after at least one byte following the match
  if( valid ) {
    maps[0].set(hash(expectedByte, shared->c0, shared->c1, (shared->c4 >> 8U) & 0xffu, ilog2(length + 1) * numHashes + hashIndex));
    const uint32_t c1ExpectedByte = (shared->c1 << 8U) | expectedByte;
    maps[1].setDirect(c1ExpectedByte);
    iCtx8 = c1ExpectedByte;
    iCtx16 = c1ExpectedByte;
    maps[2].setDirect(iCtx8());
    maps[3].setDirect(iCtx16());
  }
}

void SparseMatchModel::mix(Mixer &m) {
  const uint8_t b = shared->c0 << (8 - shared->bitPosition);
  if( shared->bitPosition == 0 ) {
    update();
  } else if( valid ) {
    maps[0].set(hash(expectedByte, shared->c0, shared->c1, (shared->c4 >> 8U) & 0xffU, ilog2(length + 1) * numHashes + hashIndex));
    if( shared->bitPosition == 4 ) {
      maps[1].setDirect(0x10000U | ((expectedByte ^ uint8_t(shared->c0 << 4U)) << 8U) | shared->c1);
    }
    INJECT_SHARED_y
    iCtx8 += y;
    iCtx8 = (shared->bitPosition << 16U) | (shared->c1 << 8U) | (expectedByte ^ b);
    maps[2].setDirect(iCtx8());
    maps[3].setDirect((shared->bitPosition << 16U) | (iCtx16() ^ uint32_t(b | (b << 8U))));
  }

  // check if next bit matches the prediction, accounting for the required bitmask
  if( length > 0 && (((expectedByte ^ b) & sparse[hashIndex].bitMask) >> (8 - shared->bitPosition)) != 0 ) {
    length = 0;
  }

  if( valid ) {
    if( length > 1 && ((sparse[hashIndex].bitMask >> (7 - shared->bitPosition)) & 1U) > 0 ) {
      const int expectedBit = (expectedByte >> (7 - shared->bitPosition)) & 1U;
      const int sign = 2 * expectedBit - 1;
      m.add(sign * (min(length - 1, 64) << 4U)); // +/- 16..1024
      m.add(sign * (1U << min(length - 2, 3)) * min(length - 1, 8) << 4U); // +/- 16..1024
      m.add(sign * 512);
    } else {
      m.add(0);
      m.add(0);
      m.add(0);
    }
    for( int i = 0; i < nSM; i++ ) {
      maps[i].mix(m);
    }
  } else {
    for( int i = 0; i < MIXERINPUTS; i++ ) {
      m.add(0);
    }
  }

  m.set((hashIndex << 6U) | (shared->bitPosition << 3) | min(7, length), numHashes * 64);
  m.set((hashIndex << 11U) | (min(7, ilog2(length + 1)) << 8) | (shared->c0 ^ (expectedByte >> (8 - shared->bitPosition))),
        numHashes * 2048);
}
