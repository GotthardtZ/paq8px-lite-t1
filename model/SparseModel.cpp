#include "SparseModel.hpp"
#include "../Hash.hpp"

SparseModel::SparseModel(const uint64_t size) : cm(size, nCM) {}

void SparseModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_buf
    INJECT_SHARED_c4
    uint64_t i = 0;
    cm.set(hash(++i, buf(1) | buf(5) << 8U));
    cm.set(hash(++i, buf(1) | buf(6) << 8U));
    cm.set(hash(++i, buf(3) | buf(6) << 8U));
    cm.set(hash(++i, buf(4) | buf(8) << 8U));
    cm.set(hash(++i, buf(1) | buf(3) << 8U | buf(5) << 16U));
    cm.set(hash(++i, buf(2) | buf(4) << 8U | buf(6) << 16U));
    cm.set(hash(++i, c4 & 0x00f0f0ffU));
    cm.set(hash(++i, c4 & 0x00ff00ffU));
    cm.set(hash(++i, c4 & 0xff0000ffU));
    cm.set(hash(++i, c4 & 0x00f8f8f8U));
    cm.set(hash(++i, c4 & 0xf8f8f8f8U));
    cm.set(hash(++i, c4 & 0x00e0e0e0U));
    cm.set(hash(++i, c4 & 0xe0e0e0e0U));
    cm.set(hash(++i, c4 & 0x810000c1U));
    cm.set(hash(++i, c4 & 0xC3CCC38CU));
    cm.set(hash(++i, c4 & 0x0081CC81U));
    cm.set(hash(++i, c4 & 0x00c10081U));
    for( int j = 2; j <= 8; ++j ) {
      cm.set(hash(++i, buf(j)));
      cm.set(hash(++i, (buf(j + 1) << 8U) | buf(j)));
      cm.set(hash(++i, (buf(j + 2) << 8U) | buf(j)));
    }
    assert((int) i == nCM);
  }
  cm.mix(m);
}
