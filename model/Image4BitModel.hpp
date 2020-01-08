#ifndef PAQ8PX_IMAGE4BITMODEL_HPP
#define PAQ8PX_IMAGE4BITMODEL_HPP

#include "../Shared.hpp"
#include "../RingBuffer.hpp"

/**
 * Model for 4-bit image data
 */
class Image4BitModel {
private:
    static constexpr int S = 14; //number of contexts
    const Shared *const shared;
    Random rnd;
    HashTable<16> t;
    StateMap sm;
    StateMap map;
    uint8_t *cp[S] {}; // context pointers
    uint8_t WW = 0, W = 0, NWW = 0, NW = 0, N = 0, NE = 0, NEE = 0, NNWW = 0, NNW = 0, NN = 0, NNE = 0, NNEE = 0;
    int w = 0, col = 0, line = 0, run = 0, prevColor = 0, px = 0;

public:
    static constexpr int MIXERINPUTS = (S * 3 + 1);
    static constexpr int MIXERCONTEXTS = 256 + 512 + 512 + 1024 + 16 + 1; //2321
    static constexpr int MIXERCONTEXTSETS = 6;
    Image4BitModel(const Shared *sh, uint64_t size);
    void setParam(int info0);
    void mix(Mixer &m);
};

Image4BitModel::Image4BitModel(const Shared *const sh, const uint64_t size) : shared(sh), t(size),
        sm {sh, S, 256, 1023, StateMap::BitHistory}, //StateMap: s, n, lim, init
        map {sh, 1, 16, 1023, StateMap::Generic} //StateMap: s, n, lim, init
{}

void Image4BitModel::setParam(int info0) {
  w = info0;
}

void Image4BitModel::mix(Mixer &m) {
  if( !cp[0] ) {
    for( int i = 0; i < S; i++ )
      cp[i] = t[263 * i]; //set the initial context to an arbitrary slot in the hashtable
  }
  INJECT_SHARED_y
  for( int i = 0; i < S; i++ )
    StateTable::update(cp[i], y, rnd); //update hashtable item priorities using predicted counts

  INJECT_SHARED_bpos
  if( bpos == 0 || bpos == 4 ) {
    WW = W;
    NWW = NW;
    NW = N;
    N = NE;
    NE = NEE;
    NNWW = NWW;
    NNW = NN;
    NN = NNE;
    NNE = NNEE;
    INJECT_SHARED_buf
    if( bpos == 0 ) {
      INJECT_SHARED_c4
      W = c4 & 0xF;
      NEE = buf(w - 1) >> 4;
      NNEE = buf(w * 2 - 1) >> 4;
    } else {
      INJECT_SHARED_c0
      W = c0 & 0xF;
      NEE = buf(w - 1) & 0xF;
      NNEE = buf(w * 2 - 1) & 0xF;
    }
    run = (W != WW || col == 0) ? (prevColor = WW, 0) : min(0xFFF, run + 1);
    px = 1; //partial pixel (4 bits) with a leading "1"
    uint64_t i = 0;
    cp[i] = t[hash(i, W, NW, N)];
    i++;
    cp[i] = t[hash(i, N, min(0xFFF, col / 8))];
    i++;
    cp[i] = t[hash(i, W, NW, N, NN, NE)];
    i++;
    cp[i] = t[hash(i, W, N, NE + NNE * 16, NEE + NNEE * 16)];
    i++;
    cp[i] = t[hash(i, W, N, NW + NNW * 16, NWW + NNWW * 16)];
    i++;
    cp[i] = t[hash(i, W, ilog2(run + 1), prevColor, col / max(1, w / 2))];
    i++;
    cp[i] = t[hash(i, NE, min(0x3FF, (col + line) / max(1, w * 8)))];
    i++;
    cp[i] = t[hash(i, NW, (col - line) / max(1, w * 8))];
    i++;
    cp[i] = t[hash(i, WW * 16 + W, NN * 16 + N, NNWW * 16 + NW)];
    i++;
    cp[i] = t[hash(i, N, NN)];
    i++;
    cp[i] = t[hash(i, W, WW)];
    i++;
    cp[i] = t[hash(i, W, NE)];
    i++;
    cp[i] = t[hash(i, WW, NN, NEE)];
    i++;
    cp[i] = t[-1];
    assert(++i == S);
    col++;
    if( col == w * 2 ) {
      col = 0;
      line++;
    }
  } else {
    INJECT_SHARED_y
    px += px + y;
    int j = (y + 1) << (bpos & 3);
    for( int i = 0; i < S; i++ )
      cp[i] += j;
  }

  // predict
  sm.subscribe();
  for( int i = 0; i < S; i++ ) {
    const uint8_t s = *cp[i];
    const int n0 = -!StateTable::next(s, 2);
    const int n1 = -!StateTable::next(s, 3);
    const int p1 = sm.p2(i, s);
    const int st = stretch(p1) >> 1;
    m.add(st);
    m.add((p1 - 2048) >> 2);
    m.add(st * abs(n1 - n0));
  }
  m.add(stretch(map.p1(px)) >> 1);

  m.set((W << 4) | px, 256);
  m.set(min(31, col / max(1, w / 16)) | (N << 5), 512);
  m.set((bpos & 3) | (W << 2) | (min(7, ilog2(run + 1)) << 6), 512);
  m.set(W | (NE << 4) | ((bpos & 3U) << 8), 1024);
  m.set(px, 16);
  m.set(0, 1);
}

#endif //PAQ8PX_IMAGE4BITMODEL_HPP
