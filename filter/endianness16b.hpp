#ifndef PAQ8PX_ENDIANNESS16B_HPP
#define PAQ8PX_ENDIANNESS16B_HPP

#include "../Encoder.hpp"
#include "../file/File.hpp"
#include "Filter.hpp"
#include <cstdint>

static void encodeEndianness16B(File *in, File *out, uint64_t size) {
  for( uint64_t i = 0, l = size >> 1U; i < l; i++ ) {
    uint8_t b = in->getchar();
    out->putChar(in->getchar());
    out->putChar(b);
  }
  if((size & 1U) > 0 )
    out->putChar(in->getchar());
}

static auto decodeEndianness16B(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) -> uint64_t {
  for( uint64_t i = 0, l = size >> 1U; i < l; i++ ) {
    uint8_t b1 = en.decompress();
    uint8_t b2 = en.decompress();
    if( mode == FDECOMPRESS ) {
      out->putChar(b2);
      out->putChar(b1);
    } else if( mode == FCOMPARE ) {
      bool ok = out->getchar() == b2;
      ok &= out->getchar() == b1;
      if( !ok && (diffFound == 0u)) {
        diffFound = size - i * 2;
        break;
      }
    }
    if( mode == FDECOMPRESS && ((i & 0x7FFU) == 0u))
      en.printStatus();
  }
  if((diffFound == 0u) && (size & 1U) > 0 ) {
    if( mode == FDECOMPRESS )
      out->putChar(en.decompress());
    else if( mode == FCOMPARE ) {
      if( out->getchar() != en.decompress())
        diffFound = size - 1;
    }
  }
  return size;
}

#endif //PAQ8PX_ENDIANNESS16B_HPP
