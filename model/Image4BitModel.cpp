#include "Image4BitModel.hpp"
#include "../Stretch.hpp"


Image4BitModel::Image4BitModel(const uint64_t size) : t(size), sm {S, 256, 1023, StateMap::BitHistory},
        map {1, 16, 1023, StateMap::Generic} {}

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

  if( shared->bitPosition == 0 || shared->bitPosition == 4 ) {
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
    if( shared->bitPosition == 0 ) {
      W = shared->c4 & 0xFU;
      NEE = buf(w - 1) >> 4U;
      NNEE = buf(w * 2 - 1) >> 4U;
    } else {
      W = shared->c0 & 0xFU;
      NEE = buf(w - 1) & 0xFU;
      NNEE = buf(w * 2 - 1) & 0xFU;
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
    int j = (y + 1) << (shared->bitPosition & 3U);
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
    const int st = stretch(p1) >> 1U;
    m.add(st);
    m.add((p1 - 2048) >> 2U);
    m.add(st * abs(n1 - n0));
  }
  m.add(stretch(map.p1(px)) >> 1U);

  m.set((W << 4U) | px, 256);
  m.set(min(31, col / max(1, w / 16)) | (N << 5U), 512);
  m.set((shared->bitPosition & 3U) | (W << 2U) | (min(7, ilog2(run + 1)) << 6U), 512);
  m.set(W | (NE << 4U) | ((shared->bitPosition & 3U) << 8U), 1024);
  m.set(px, 16);
  m.set(0, 1);
}
