#include "NormalModel.hpp"

NormalModel::NormalModel(ModelStats *st, const uint64_t cmSize) : stats(st), cm(cmSize, nCM, 64, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY),
        smOrder0Slow(1, 255, 1023, StateMap::Generic), smOrder1Slow(1, 255 * 256, 1023, StateMap::Generic),
        smOrder1Fast(1, 255 * 256, 64, StateMap::Generic) // 64->16 is also ok
{
  assert(isPowerOf2(cmSize));
}

void NormalModel::reset() {
  memset(&cxt[0], 0, sizeof(cxt));
}

void NormalModel::updateHashes() {
  INJECT_SHARED_c1
  for( int i = 14; i > 0; --i )
    cxt[i] = combine64(cxt[i - 1], c1 + (i << 10));
}

void NormalModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    updateHashes();
    for( int i = 1; i <= 7; ++i )
      cm.set(cxt[i]);
    cm.set(cxt[9]);
    cm.set(cxt[14]);
  }
  cm.mix(m);

  INJECT_SHARED_c0
  m.add((stretch(smOrder0Slow.p1(c0 - 1))) >> 2); //order 0
  INJECT_SHARED_c1
  m.add((stretch(smOrder1Fast.p1((c0 - 1) << 8 | c1))) >> 2); //order 1
  m.add((stretch(smOrder1Slow.p1((c0 - 1) << 8 | c1))) >> 2); //order 1

  const int order = max(0, cm.order - (nCM - 7)); //0-7
  assert(0 <= order && order <= 7);
  m.set(order << 3U | bpos, 64);
  stats->order = order;
}

void NormalModel::mixPost(Mixer &m) {
  INJECT_SHARED_c4
  uint32_t c2 = (c4 >> 8U) & 0xff, c3 = (c4 >> 16U) & 0xff, c;

  INJECT_SHARED_c0
  INJECT_SHARED_c1
  INJECT_SHARED_bpos
  m.set(8 + (c1 | (bpos > 5) << 8 | (((c0 & ((1 << bpos) - 1)) == 0) || (c0 == ((2 << bpos) - 1))) << 9), 8 + 1024);
  m.set(c0, 256);
  m.set(stats->order | ((c4 >> 6) & 3) << 3 | (bpos == 0) << 5 | (c1 == c2) << 6 | (stats->blockType == EXE) << 7, 256);
  m.set(c2, 256);
  m.set(c3, 256);
  if( bpos != 0 ) {
    c = c0 << (8 - bpos);
    if( bpos == 1 )
      c |= c3 >> 1;
    c = min(bpos, 5) << 8 | c1 >> 5 | (c2 >> 5) << 3 | (c & 192);
  } else
    c = c3 >> 7 | (c4 >> 31) << 1 | (c2 >> 6) << 2 | (c1 & 240);
  m.set(c, 1536);
}
