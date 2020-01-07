#ifndef PAQ8PX_EOL_HPP
#define PAQ8PX_EOL_HPP

// EOL transform

void encodeEol(File *in, File *out, uint64_t len) {
  uint8_t b = 0, pB = 0;
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

uint64_t decodeEol(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t b;
  uint64_t count = 0;
  for( int i = 0; i < (int) size; i++, count++ ) {
    if((b = en.decompress()) == NEW_LINE ) {
      if( mode == FDECOMPRESS )
        out->putChar(CARRIAGE_RETURN);
      else if( mode == FCOMPARE ) {
        if( out->getchar() != CARRIAGE_RETURN && !diffFound ) {
          diffFound = size - i;
          break;
        }
      }
      count++;
    }
    if( mode == FDECOMPRESS )
      out->putChar(b);
    else if( mode == FCOMPARE ) {
      if( b != out->getchar() && !diffFound ) {
        diffFound = size - i;
        break;
      }
    }
    if( mode == FDECOMPRESS && !(i & 0xFFFU))
      en.printStatus();
  }
  return count;
}

#endif //PAQ8PX_EOL_HPP
