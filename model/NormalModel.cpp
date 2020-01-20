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
  for( int i = 14; i > 0; --i ) {
    cxt[i] = combine64(cxt[i - 1], shared->c1 + (i << 10U));
  }
}

void NormalModel::mix(Mixer &m) {
  if( shared->bitPosition == 0 ) {
    updateHashes();
    for( int i = 1; i <= 7; ++i ) {
      cm.set(cxt[i]);
    }
    cm.set(cxt[9]);
    cm.set(cxt[14]);
  }
  cm.mix(m);

  m.add((stretch(smOrder0Slow.p1(shared->c0 - 1))) >> 2U); //order 0
  m.add((stretch(smOrder1Fast.p1((shared->c0 - 1) << 8U | shared->c1))) >> 2U); //order 1
  m.add((stretch(smOrder1Slow.p1((shared->c0 - 1) << 8U | shared->c1))) >> 2U); //order 1

  const int order = max(0, cm.order - (nCM - 7)); //0-7
  assert(0 <= order && order <= 7);
  m.set(order << 3U | shared->bitPosition, 64);
  stats->order = order;
}

void NormalModel::mixPost(Mixer &m) {
  uint32_t c2 = (shared->c4 >> 8U) & 0xffU, c3 = (shared->c4 >> 16U) & 0xffU, c;

  m.set(8 + (shared->c1 | static_cast<int>(shared->bitPosition > 5) << 8U |
             static_cast<int>(((shared->c0 & ((1U << shared->bitPosition) - 1)) == 0) || (shared->c0 == ((2 << shared->bitPosition) - 1)))
                     << 9U), 8 + 1024);
  m.set(shared->c0, 256);
  m.set(stats->order | ((shared->c4 >> 6U) & 3U) << 3U | static_cast<int>(shared->bitPosition == 0) << 5U |
        static_cast<int>(shared->c1 == c2) << 6U | static_cast<int>(stats->blockType == EXE) << 7U, 256);
  m.set(c2, 256);
  m.set(c3, 256);
  if( shared->bitPosition != 0 ) {
    c = shared->c0 << (8 - shared->bitPosition);
    if( shared->bitPosition == 1 ) {
      c |= c3 >> 1U;
    }
    c = min(shared->bitPosition, 5) << 8U | shared->c1 >> 5U | (c2 >> 5U) << 3U | (c & 192U);
  } else {
    c = c3 >> 7 | (shared->c4 >> 31U) << 1U | (c2 >> 6U) << 2U | (shared->c1 & 240U);
  }
  m.set(c, 1536);
}
