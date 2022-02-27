#include "NormalModel.hpp"

NormalModel::NormalModel(Shared* const sh, const uint64_t cmSize) :
  shared(sh), 
  cm(sh, cmSize),
  smOrder0(sh, 255, 4, StateMap::Generic),
  smOrder1(sh, 255 * 256, 32, StateMap::Generic),
  smOrder2(sh, 1<<24, 1023, StateMap::Generic)
{
  assert(isPowerOf2(cmSize));
}

bool isSegmentBorder(uint32_t c3) {
  static constexpr uint32_t SEGMENT_BORDER_MARKERS[]{ 
    0xEFBC8C,0xE79A84,0xE38082,0xE38081,0xEFBC88,0xEFBC89,0xE59CA8,0xE698AF,
    0xE69C89,0xE5928C,0xEFBC9A,0xE782BA,0xE4BBA5,0xE3808A,0xE4BA86,0xE696BC,
    0xE4B8BA,0xE794A8,0xE3808C,0xE58F8A,0xE8808C,0xE588B0,0xE4BA8E,0xE794B1,
    0xE8A2AB,0xE88887,0xE4BBBB,0xE59091,0xE5808B,0xE2809C,0xE887B3,0xE88085,
    0xE6ADA4,0xE585A5,0xE4BD86,0xEFBC9B,0xE4B88E,0xE68896,0xE5A682,0xE5B087,
    0xE68BAC,0xE4BA9B,0xE4B894,
  };
  constexpr size_t SEGMENT_BORDER_MARKER_COUNT = sizeof(SEGMENT_BORDER_MARKERS)/sizeof(uint32_t);
  for (size_t i = 0; i < SEGMENT_BORDER_MARKER_COUNT; i++)
    if (SEGMENT_BORDER_MARKERS[i] == c3)
      return true;
  return false;
}

void NormalModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  INJECT_SHARED_c1
  if( bpos == 0 ) {

    uint64_t lastchar = static_cast<uint64_t>(c1);
    lastchar++;
    if (utf8left == 0) {

      utf8c7 = (utf8c6 + lastchar) * PHI64;
      utf8c6 = (utf8c5 + lastchar) * PHI64;
      utf8c5 = (utf8c4 + lastchar) * PHI64;
      utf8c4 = (utf8c3 + lastchar) * PHI64;
      utf8c3 = (utf8c2 + lastchar) * PHI64;
      utf8c2 = (utf8c1 + lastchar) * PHI64;
      utf8c1 = (lastchar) * PHI64; // first byte of a UTF8 character, might be ascii

      if ((c1 >> 5) == 0b110) utf8left = 1;
      else if ((c1 >> 4) == 0b1110) utf8left = 2;
      else if ((c1 >> 3) == 0b11110) utf8left = 3;
      else utf8left = 0; //ascii or utf8 error

    }
    else {

      utf8c7 = (utf8c7 + lastchar) * PHI64;
      utf8c6 = (utf8c6 + lastchar) * PHI64;
      utf8c5 = (utf8c5 + lastchar) * PHI64;
      utf8c4 = (utf8c4 + lastchar) * PHI64;
      utf8c3 = (utf8c3 + lastchar) * PHI64;
      utf8c2 = (utf8c2 + lastchar) * PHI64;
      utf8c1 = (utf8c1 + lastchar) * PHI64;

      utf8left--;
      if ((c1 >> 6) != 0b10)
        utf8left = 0; //utf8 error
    }

    lastByteType =
      c1 >= '0' && c1 <= '9' ? 0 :
      (c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') ? 1 :
      (c1 < 128) ? 2 :
      3 + utf8left; //0..6

    cm.set(0, utf8c1);
    cm.set(1, utf8c2);
    cm.set(2, utf8c3);
    cm.set(3, utf8c4);
    cm.set(4, utf8c5);
    cm.set(5, utf8c6);
    cm.set(6, utf8c7);

    uint8_t tokentype = lastByteType <= 1 ? 0 : lastByteType == 2 ? 1 : 2;
    INJECT_SHARED_c4
    const uint32_t c3 = c4 & 0xffffff;
    const bool isSegmentStart = (utf8left == 0 && (c3 & 0xE0C0C0) == 0xE08080 && isSegmentBorder(c3));
    if (isSegmentStart) {
      tokenHash = MUL64_1;
      tokentype = 3;
    }
    else {
      if ((tokentype != lasttokentype))
        tokenHash = MUL64_1;
      tokenHash = (tokenHash + lastchar) * PHI64;
    }
    cm.set(7, tokenHash);
    lasttokentype = tokentype;
  }
  cm.mix(m);
  
  INJECT_SHARED_c0
  int p1, st;
  p1 = smOrder0.p1(c0 - 1);
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);

  uint32_t c = ((c1 & 0xc0) == 0x80 ? 0x80 + utf8left : c1);
  p1 = smOrder1.p1((c0 - 1) << 8 | c); 
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);

  p1 = smOrder2.p1(finalize64(utf8c1, 24) ^ c0);
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);

  //modeling significant bits (utf8)
  st = 0;
  if (bpos == 0) {
    if (utf8left != 0)
      st = 2047;
  }
  else if (bpos == 1) {
    if (utf8left != 0)
      st = -2047;
    else if ((c0 & 1) == 1)
      st = 2047;
  }
  m.add(st);

  //modeling switching between ascii and non-ascii fragments
  st = 0;
  if (bpos == 0) {
    if (c1 >= 128)
      st = +512;
    else
      st = -512;
  }
  m.add(st);


  uint32_t misses = shared->State.misses << ((8 - bpos) & 7); //byte-aligned
  misses = (misses & 0xffffff00) | (misses & 0xff) >> ((8 - bpos) & 7);

  uint32_t misses3 =
    ((misses & 0x1) != 0) |
    ((misses & 0xfe) != 0) << 1 |
    ((misses & 0xff00) != 0) << 2;
   
  const int order = cm.order; //0..6 (C)
  m.set((order * 7 + lastByteType) << 3 | bpos, (ContextMap2::C + 1) * 8 * 7);
  m.set(((misses3) * 7 + lastByteType) * 255 + (c0 - 1), 255 * 8 * 7);
  m.set(order << 10 | (misses != 0) << 9 | (utf8left == 0) << 8 | c1, (ContextMap2::C + 1) * 2 * 2 * 256);
  m.set(cm.confidence, 6561); // 3^8

}

