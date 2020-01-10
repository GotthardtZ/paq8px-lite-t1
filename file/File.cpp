#include "File.hpp"

void File::append(const char *s) {
  for( int i = 0; s[i]; i++ )
    putChar((uint8_t) s[i]);
}

uint32_t File::get32() { return (getchar() << 24U) | (getchar() << 16U) | (getchar() << 8U) | (getchar()); }

void File::put32(uint32_t x) {
  putChar((x >> 24U) & 255U);
  putChar((x >> 16U) & 255U);
  putChar((x >> 8U) & 255U);
  putChar(x & 255U);
}

uint64_t File::getVLI() {
  uint64_t i = 0;
  int k = 0;
  uint8_t b = 0;
  do {
    b = getchar();
    i |= uint64_t((b & 0x7FU) << k);
    k += 7;
  } while((b >> 7) > 0 );
  return i;
}

void File::putVLI(uint64_t i) {
  while( i > 0x7F ) {
    putChar(0x80U | (i & 0x7FU));
    i >>= 7U;
  }
  putChar(uint8_t(i));
}
