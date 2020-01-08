#ifndef PAQ8PX_IMAGE1BITMODEL_HPP
#define PAQ8PX_IMAGE1BITMODEL_HPP

#include "../Shared.hpp"
#include "../RingBuffer.hpp"

/**
 * Model for 1-bit image data
 */
class Image1BitModel {
private:
    static constexpr int s = 11;
    const Shared *const shared;
    Random rnd;
    int w = 0;
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0; // last 4 rows, bit 8 is over current pixel
    Array<uint8_t> t {0x23000}; // model: cxt -> state
    int cxt[s] {}; // contexts
    StateMap sm;

public:
    static constexpr int MIXERINPUTS = s;
    Image1BitModel(const Shared *sh);
    void setParam(int info0);
    void mix(Mixer &m);
};

Image1BitModel::Image1BitModel(const Shared *const sh) : shared(sh),
        sm {sh, s, 256, 1023, StateMap::BitHistory} // StateMap: s, n, limit, init
{}

void Image1BitModel::setParam(int info0) {
  w = info0;
}

void Image1BitModel::mix(Mixer &m) {
  // update the model
  INJECT_SHARED_y
  for( int i = 0; i < s; ++i )
    StateTable::update(&t[cxt[i]], y, rnd);

  INJECT_SHARED_bpos
  INJECT_SHARED_buf
  // update the contexts (pixels surrounding the predicted one)
  r0 += r0 + y;
  r1 += r1 + ((buf(w - 1) >> (7 - bpos)) & 1);
  r2 += r2 + ((buf(w + w - 1) >> (7 - bpos)) & 1);
  r3 += r3 + ((buf(w + w + w - 1) >> (7 - bpos)) & 1);
  cxt[0] = (r0 & 0x7) | (r1 >> 4 & 0x38) | (r2 >> 3 & 0xc0);
  cxt[1] = 0x100 + ((r0 & 1) | (r1 >> 4 & 0x3e) | (r2 >> 2 & 0x40) | (r3 >> 1 & 0x80));
  cxt[2] = 0x200 + ((r0 & 1) | (r1 >> 4 & 0x1d) | (r2 >> 1 & 0x60) | (r3 & 0xC0));
  cxt[3] = 0x300 + (y | ((r0 << 1) & 4) | ((r1 >> 1) & 0xF0) | ((r2 >> 3) & 0xA));
  cxt[4] = 0x400 + ((r0 >> 4 & 0x2AC) | (r1 & 0xA4) | (r2 & 0x349) | (!(r3 & 0x14D)));
  cxt[5] = 0x800 + (y | ((r1 >> 4) & 0xE) | ((r2 >> 1) & 0x70) | ((r3 << 2) & 0x380));
  cxt[6] = 0xC00 + (((r1 & 0x30) ^ (r3 & 0x0c0c)) | (r0 & 3));
  cxt[7] = 0x1000 + ((!(r0 & 0x444)) | (r1 & 0xC0C) | (r2 & 0xAE3) | (r3 & 0x51C));
  cxt[8] = 0x2000 + ((r0 & 7) | ((r1 >> 1) & 0x3F8) | ((r2 << 5) & 0xC00));
  cxt[9] = 0x3000 + ((r0 & 0x3f) ^ (r1 & 0x3ffe) ^ (r2 << 2 & 0x7f00) ^ (r3 << 5 & 0xf800));
  cxt[10] = 0x13000 + ((r0 & 0x3e) ^ (r1 & 0x0c0c) ^ (r2 & 0xc800));

  // predict
  sm.subscribe();
  for( int i = 0; i < s; ++i ) {
    const uint8_t s = t[cxt[i]];
    m.add(stretch(sm.p2(i, s)));
  }
}

#endif //PAQ8PX_IMAGE1BITMODEL_HPP
