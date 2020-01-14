#ifndef PAQ8PX_EOL_HPP
#define PAQ8PX_EOL_HPP

#include "../file/File.hpp"
#include "../Encoder.hpp"
#include "Filter.hpp"
#include <cstdint>

// EOL transform

static void encodeEol(File *in, File *out, uint64_t len) {
  uint8_t b = 0;
  uint8_t pB = 0;
  for( int i = 0; i < (int) len; i++ ) {
    b = in->getchar();
    if( pB == CARRIAGE_RETURN && b != NEW_LINE )
      out->putChar(pB);
    if( b != CARRIAGE_RETURN )
      out->putChar(b);
    pB = b;
  }
  if( b == CARRIAGE_RETURN )
    out->putChar(b);
}

static auto decodeEol(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) -> uint64_t {
  uint8_t b = 0;
  uint64_t count = 0;
  for( int i = 0; i < (int) size; i++, count++ ) {
    if((b = en.decompress()) == NEW_LINE ) {
      if( mode == FDECOMPRESS )
        out->putChar(CARRIAGE_RETURN);
      else if( mode == FCOMPARE ) {
        if( out->getchar() != CARRIAGE_RETURN && (diffFound == 0u)) {
          diffFound = size - i;
          break;
        }
      }
      count++;
    }
    if( mode == FDECOMPRESS )
      out->putChar(b);
    else if( mode == FCOMPARE ) {
      if( b != out->getchar() && (diffFound == 0u)) {
        diffFound = size - i;
        break;
      }
    }
    if( mode == FDECOMPRESS && ((i & 0xFFFU) == 0u))
      en.printStatus();
  }
  return count;
}

#endif //PAQ8PX_EOL_HPP
