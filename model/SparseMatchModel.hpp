#ifndef PAQ8PX_SPARSEMATCHMODEL_HPP
#define PAQ8PX_SPARSEMATCHMODEL_HPP

#include "../MTFList.hpp"

class SparseMatchModel {
private:
    static constexpr int numHashes = 4;
    static constexpr int nSM = 4;

public:
    static constexpr int MIXERINPUTS = 3 + nSM * StationaryMap::MIXERINPUTS; // 11
    static constexpr int MIXERCONTEXTS = numHashes * (64 + 2048); // 8448
    static constexpr int MIXERCONTEXTSETS = 2;

private:
    const Shared *const shared;
    enum Parameters : uint32_t {
        MaxLen = 0xFFFF, // longest allowed match
        MinLen = 3, // default minimum required match length
    };
    struct SparseConfig {
        uint32_t offset = 0; // number of last input bytes to ignore when searching for a match
        uint32_t stride = 1; // look for a match only every stride bytes after the offset
        uint32_t deletions = 0; // when a match is found, ignore these many initial post-match bytes, to model deletions
        uint32_t minLen = MinLen;
        uint32_t bitMask = 0xFF; // match every byte according to this bit mask
    };
    const SparseConfig sparse[numHashes] = {{0, 1, 0, 5, 0xDF},
                                            {1, 1, 0, 4, 0xFF},
                                            {0, 2, 0, 4, 0xDF},
                                            {0, 1, 0, 5, 0x0F}};
    Array<uint32_t> table;
    StationaryMap maps[nSM];
    IndirectContext<uint8_t> iCtx8 {19, 1}; // BitsPerContext, InputBits
    IndirectContext<uint16_t> iCtx16 {16, 8}; // BitsPerContext, InputBits
    MTFList list {numHashes};
    uint32_t hashes[numHashes] {};
    uint32_t hashIndex = 0; // index of hash used to find current match
    uint32_t length = 0; // rebased length of match (length=1 represents the smallest accepted match length), or 0 if no match
    uint32_t index = 0; // points to next byte of match in buf, 0 when there is no match
    uint8_t expectedByte = 0; // prediction is based on this byte (buf[index]), valid only when length>0
    bool valid = false;
    const uint32_t mask;
    const int hashBits;

public:
    SparseMatchModel(const Shared *const sh, const uint64_t size) : shared(sh), table(size / sizeof(uint32_t)),
            maps {{sh, 22, 1, 128, 1023},
                  {sh, 17, 4, 128, 1023},
                  {sh, 8,  1, 128, 1023},
                  {sh, 19, 1, 128, 1023}}, mask(uint32_t(size / sizeof(uint32_t) - 1)), hashBits(ilog2(mask + 1)) {
      assert(isPowerOf2(size));
    }

    void update() {
      INJECT_SHARED_buf
      // update sparse hashes
      for( uint32_t i = 0; i < numHashes; i++ ) {
        uint64_t hash = 0;
        for( uint32_t j = 0, k = sparse[i].offset + 1; j < sparse[i].minLen; j++, k += sparse[i].stride )
          hash = combine64(hash, buf(k) & sparse[i].bitMask);
        hashes[i] = finalize64(hash, hashBits);
      }
      // extend current match, if available
      if( length ) {
        index++;
        if( length < MaxLen )
          length++;
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
      INJECT_SHARED_pos
      for( uint32_t i = 0; i < numHashes; i++ )
        table[hashes[i]] = pos;

      expectedByte = length != 0 ? buf[index] : 0;
      INJECT_SHARED_c1
      if( valid ) {
        INJECT_SHARED_y
        iCtx8 += y;
        iCtx16 += c1;
      }
      valid = length > 1; // only predict after at least one byte following the match
      if( valid ) {
        INJECT_SHARED_c0
        INJECT_SHARED_c4
        maps[0].set(hash(expectedByte, c0, c1, (c4 >> 8U) & 0xffu, ilog2(length + 1) * numHashes + hashIndex));
        const uint32_t c1ExpectedByte = (c1 << 8U) | expectedByte;
        maps[1].setDirect(c1ExpectedByte);
        iCtx8 = c1ExpectedByte;
        iCtx16 = c1ExpectedByte;
        maps[2].setDirect(iCtx8());
        maps[3].setDirect(iCtx16());
      }
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      INJECT_SHARED_c0
      const uint8_t b = c0 << (8 - bpos);
      if( bpos == 0 )
        update();
      else if( valid ) {
        INJECT_SHARED_c1
        INJECT_SHARED_c4
        maps[0].set(hash(expectedByte, c0, c1, (c4 >> 8) & 0xff, ilog2(length + 1) * numHashes + hashIndex));
        if( bpos == 4 )
          maps[1].setDirect(0x10000 | ((expectedByte ^ uint8_t(c0 << 4)) << 8) | c1);
        INJECT_SHARED_y
        iCtx8 += y;
        iCtx8 = (bpos << 16) | (c1 << 8) | (expectedByte ^ b);
        maps[2].setDirect(iCtx8());
        maps[3].setDirect((bpos << 16) | (iCtx16() ^ uint32_t(b | (b << 8))));
      }

      // check if next bit matches the prediction, accounting for the required bitmask
      if( length > 0 && (((expectedByte ^ b) & sparse[hashIndex].bitMask) >> (8 - bpos)) != 0 )
        length = 0;

      if( valid ) {
        if( length > 1 && ((sparse[hashIndex].bitMask >> (7 - bpos)) & 1) > 0 ) {
          const int expectedBit = (expectedByte >> (7 - bpos)) & 1;
          const int sign = 2 * expectedBit - 1;
          m.add(sign * (min(length - 1, 64) << 4)); // +/- 16..1024
          m.add(sign * (1 << min(length - 2, 3)) * min(length - 1, 8) << 4); // +/- 16..1024
          m.add(sign * 512);
        } else {
          m.add(0);
          m.add(0);
          m.add(0);
        }
        for( int i = 0; i < nSM; i++ ) {
          maps[i].mix(m);
        }
      } else
        for( int i = 0; i < MIXERINPUTS; i++ )
          m.add(0);

      m.set((hashIndex << 6) | (bpos << 3) | min(7, length), numHashes * 64);
      m.set((hashIndex << 11) | (min(7, ilog2(length + 1)) << 8) | (c0 ^ (expectedByte >> (8 - bpos))), numHashes * 2048);
    }
};

#endif //PAQ8PX_SPARSEMATCHMODEL_HPP
