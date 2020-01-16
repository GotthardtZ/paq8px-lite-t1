#include "CharGroupModel.hpp"

CharGroupModel::CharGroupModel(const uint64_t size) : cm(size, nCM, 64, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY) {}

void CharGroupModel::mix(Mixer &m) {
  if( shared->bitPosition == 0 ) {
    uint32_t g = shared->c1; // group identifier
    if( '0' <= g && g <= '9' )
      g = '0'; //all digits are in one group
    else if( 'A' <= g && g <= 'Z' )
      g = 'A'; //all uppercase letters are in one group
    else if( 'a' <= g && g <= 'z' )
      g = 'a'; //all lowercase letters are in one group
    else if( g >= 128 )
      g = 128;

    const bool toBeCollapsed = (g == '0' || g == 'A' || g == 'a') && (g == (gAscii1 & 0xffu));
    if( !toBeCollapsed ) {
      gAscii3 <<= 8U;
      gAscii3 |= gAscii2 >> (32U - 8U);
      gAscii2 <<= 8U;
      gAscii2 |= gAscii1 >> (32U - 8U);
      gAscii1 <<= 8U;
      gAscii1 |= g;
    }

    uint64_t i = toBeCollapsed * 8;
    cm.set(hash((++i), gAscii3, gAscii2, gAscii1)); // last 12 groups
    cm.set(hash((++i), gAscii2, gAscii1)); // last 8 groups
    cm.set(hash((++i), gAscii2 & 0xffffu, gAscii1)); // last 6 groups
    cm.set(hash((++i), gAscii1)); // last 4 groups
    cm.set(hash((++i), gAscii1 & 0xffffu)); // last 2 groups
    cm.set(hash((++i), gAscii2 & 0xffffffu, gAscii1, shared->c4 & 0x0000ffffu)); // last 7 groups + last 2 chars
    cm.set(hash((++i), gAscii2 & 0xffu, gAscii1, shared->c4 & 0x00ffffffu)); // last 5 groups + last 3 chars
  }
  cm.mix(m);
}
