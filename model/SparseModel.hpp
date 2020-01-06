#ifndef PAQ8PX_SPARSEMODEL_HPP
#define PAQ8PX_SPARSEMODEL_HPP

#include <cassert>
#include <cstdint>

/**
 * Model order 1-2-3 contexts with gaps.
 */
class SparseModel {
private:
    static constexpr int nCM = 38; //17+3*7
    const Shared *const shared;
    ContextMap cm;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 190
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    SparseModel(const Shared *sh, uint64_t size);
    void mix(Mixer &m);
};

void SparseModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_buf
    INJECT_SHARED_c4
    uint64_t i = 0;
    cm.set(hash(++i, buf(1) | buf(5) << 8));
    cm.set(hash(++i, buf(1) | buf(6) << 8));
    cm.set(hash(++i, buf(3) | buf(6) << 8));
    cm.set(hash(++i, buf(4) | buf(8) << 8));
    cm.set(hash(++i, buf(1) | buf(3) << 8 | buf(5) << 16));
    cm.set(hash(++i, buf(2) | buf(4) << 8 | buf(6) << 16));
    cm.set(hash(++i, c4 & 0x00f0f0ff));
    cm.set(hash(++i, c4 & 0x00ff00ff));
    cm.set(hash(++i, c4 & 0xff0000ff));
    cm.set(hash(++i, c4 & 0x00f8f8f8));
    cm.set(hash(++i, c4 & 0xf8f8f8f8));
    cm.set(hash(++i, c4 & 0x00e0e0e0));
    cm.set(hash(++i, c4 & 0xe0e0e0e0));
    cm.set(hash(++i, c4 & 0x810000c1));
    cm.set(hash(++i, c4 & 0xC3CCC38C));
    cm.set(hash(++i, c4 & 0x0081CC81));
    cm.set(hash(++i, c4 & 0x00c10081));
    for( int j = 2; j <= 8; ++j ) {
      cm.set(hash(++i, buf(j)));
      cm.set(hash(++i, (buf(j + 1) << 8) | buf(j)));
      cm.set(hash(++i, (buf(j + 2) << 8) | buf(j)));
    }
    assert((int) i == nCM);
  }
  cm.mix(m);
}

SparseModel::SparseModel(const Shared *const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM) {}

#endif //PAQ8PX_SPARSEMODEL_HPP
