#include "File.hpp"

void File::append(const char *s) {
  for( int i = 0; s[i] != 0; i++ ) {
    putChar(static_cast<uint8_t>(s[i]));
  }
}

auto File::getVLI() -> uint64_t {
  uint64_t i = 0;
  int k = 0;
  uint8_t b = 0;
  do {
    b = getchar();
    i |= uint64_t((b & 0x7FU) << k);
    k += 7;
  } while((b >> 7U) > 0 );
  return i;
}

void File::putVLI(uint64_t i) {
  while( i > 0x7F ) {
    putChar(0x80U | (i & 0x7FU));
    i >>= 7U;
  }
  putChar(uint8_t(i));
}
