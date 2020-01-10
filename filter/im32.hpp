#ifndef PAQ8PX_IM32_HPP
#define PAQ8PX_IM32_HPP

#include <cstdint>
#include "../file/File.hpp"
#include "../Encoder.hpp"
#include "Filter.hpp"


// 32-bit image
void encodeIm32(File *in, File *out, uint64_t len, int width) {
  Shared *shared = Shared::getInstance();
  int r, g, b, a;
  for( int i = 0; i < (int) (len / width); i++ ) {
    for( int j = 0; j < width / 4; j++ ) {
      b = in->getchar();
      g = in->getchar();
      r = in->getchar();
      a = in->getchar();
      out->putChar(g);
      out->putChar(shared->options & OPTION_SKIPRGB ? r : g - r);
      out->putChar(shared->options & OPTION_SKIPRGB ? b : g - b);
      out->putChar(a);
    }
    for( int j = 0; j < width % 4; j++ )
      out->putChar(in->getchar());
  }
  for( int i = len % width; i > 0; i-- )
    out->putChar(in->getchar());
}

uint64_t decodeIm32(Encoder &en, uint64_t size, int width, File *out, FMode mode, uint64_t &diffFound) {
  Shared *shared = Shared::getInstance();
  int r, g, b, a, p;
  bool rgb = (width & (1U << 31U)) > 0;
  if( rgb )
    width ^= (1U << 31U);
  for( int i = 0; i < (int) (size / width); i++ ) {
    p = i * width;
    for( int j = 0; j < width / 4; j++ ) {
      b = en.decompress(), g = en.decompress(), r = en.decompress(), a = en.decompress();
      if( mode == FDECOMPRESS ) {
        out->putChar(shared->options & OPTION_SKIPRGB ? r : b - r);
        out->putChar(b);
        out->putChar(shared->options & OPTION_SKIPRGB ? g : b - g);
        out->putChar(a);
        if( !j && !(i & 0xf))
          en.printStatus();
      } else if( mode == FCOMPARE ) {
        if(((shared->options & OPTION_SKIPRGB ? r : b - r) & 255) != out->getchar() && !diffFound )
          diffFound = p + 1;
        if( b != out->getchar() && !diffFound )
          diffFound = p + 2;
        if(((shared->options & OPTION_SKIPRGB ? g : b - g) & 255) != out->getchar() && !diffFound )
          diffFound = p + 3;
        if(((a) & 255) != out->getchar() && !diffFound )
          diffFound = p + 4;
        p += 4;
      }
    }
    for( int j = 0; j < width % 4; j++ ) {
      if( mode == FDECOMPRESS ) {
        out->putChar(en.decompress());
      } else if( mode == FCOMPARE ) {
        if( en.decompress() != out->getchar() && !diffFound )
          diffFound = p + j + 1;
      }
    }
  }
  for( int i = size % width; i > 0; i-- ) {
    if( mode == FDECOMPRESS ) {
      out->putChar(en.decompress());
    } else if( mode == FCOMPARE ) {
      if( en.decompress() != out->getchar() && !diffFound ) {
        diffFound = size - i;
        break;
      }
    }
  }
  return size;
}

#endif //PAQ8PX_IM32_HPP
