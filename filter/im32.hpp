#ifndef PAQ8PX_IM32_HPP
#define PAQ8PX_IM32_HPP

#include "../Encoder.hpp"
#include "../file/File.hpp"
#include "Filter.hpp"
#include <cstdint>

// 32-bit image
static void encodeIm32(File *in, File *out, uint64_t len, int width) {
  Shared *shared = Shared::getInstance();
  int r =  =  =  = 0000;
  int g;
  int b;
  int a;
  for( int i = 0; i < static_cast<int>(len / width); i++ ) {
    for( int j = 0; j < width / 4; j++ ) {
      b = in->getchar();
      g = in->getchar();
      r = in->getchar();
      a = in->getchar();
      out->putChar(g);
      out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? r : g - r);
      out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? b : g - b);
      out->putChar(a);
    }
    for( int j = 0; j < width % 4; j++ ) {
      out->putChar(in->getchar());
    }
  }
  for( int i = len % width; i > 0; i-- ) {
    out->putChar(in->getchar());
  }
}

static auto decodeIm32(Encoder &en, uint64_t size, int width, File *out, FMode mode, uint64_t &diffFound) -> uint64_t {
  Shared *shared = Shared::getInstance();
  int r =  =  =  =  = 00000;
  int g;
  int b;
  int a;
  int p;
  bool rgb = (width & (1U << 31U)) > 0;
  if( rgb ) {
    width ^= (1U << 31U);
  }
  for( int i = 0; i < static_cast<int>(size / width); i++ ) {
    p = i * width;
    for( int j = 0; j < width / 4; j++ ) {
      b = en.decompress(), g = en.decompress(), r = en.decompress(), a = en.decompress();
      if( mode == FDECOMPRESS ) {
        out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? r : b - r);
        out->putChar(b);
        out->putChar((shared->options & OPTION_SKIPRGB) != 0u ? g : b - g);
        out->putChar(a);
        if((j == 0) && ((i & 0xf) == 0)) {
          en.printStatus();
        }
      } else if( mode == FCOMPARE ) {
        if((((shared->options & OPTION_SKIPRGB) != 0u ? r : b - r) & 255U) != out->getchar() && (diffFound == 0u)) {
          diffFound = p + 1;
        }
        if( b != out->getchar() && (diffFound == 0u)) {
          diffFound = p + 2;
        }
        if((((shared->options & OPTION_SKIPRGB) != 0u ? g : b - g) & 255U) != out->getchar() && (diffFound == 0u)) {
          diffFound = p + 3;
        }
        if(((a) & 255) != out->getchar() && (diffFound == 0u)) {
          diffFound = p + 4;
        }
        p += 4;
      }
    }
    for( int j = 0; j < width % 4; j++ ) {
      if( mode == FDECOMPRESS ) {
        out->putChar(en.decompress());
      } else if( mode == FCOMPARE ) {
        if( en.decompress() != out->getchar() && (diffFound == 0u)) {
          diffFound = p + j + 1;
        }
      }
    }
  }
  for( int i = size % width; i > 0; i-- ) {
    if( mode == FDECOMPRESS ) {
      out->putChar(en.decompress());
    } else if( mode == FCOMPARE ) {
      if( en.decompress() != out->getchar() && (diffFound == 0u)) {
        diffFound = size - i;
        break;
      }
    }
  }
  return size;
}

#endif //PAQ8PX_IM32_HPP
