#ifndef PAQ8PX_RLE_HPP
#define PAQ8PX_RLE_HPP

void encodeRle(File *in, File *out, uint64_t size, int info, int &hdrsize) {
  uint8_t b, c = in->getchar();
  int i = 1, maxBlockSize = info & 0xFFFFFFU;
  out->putVLI(maxBlockSize);
  hdrsize = VLICost(maxBlockSize);
  while( i < (int) size ) {
    b = in->getchar(), i++;
    if( c == 0x80 ) {
      c = b;
      continue;
    } else if( c > 0x7F ) {
      for( int j = 0; j <= (c & 0x7F); j++ )
        out->putChar(b);
      c = in->getchar(), i++;
    } else {
      for( int j = 0; j <= c; j++, i++ ) {
        out->putChar(b), b = in->getchar();
      }
      c = b;
    }
  }
}

#define RLE_OUTPUT_RUN \
  { \
    while (run > 128) { \
      *outPtr++ = 0xFF, *outPtr++ = byte; \
      run -= 128; \
    } \
    *outPtr++ = (uint8_t)(0x80 | (run - 1)), *outPtr++ = byte; \
  }

uint64_t decodeRle(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t inBuffer[0x10000] = {0};
  uint8_t outBuffer[0x10200] = {0};
  uint64_t pos = 0;
  int maxBlockSize = (int) in->getVLI();
  enum {
      BASE, LITERAL, RUN, LITERAL_RUN
  } state;
  do {
    uint64_t remaining = in->blockRead(&inBuffer[0], maxBlockSize);
    auto *inPtr = (uint8_t *) inBuffer;
    auto *outPtr = (uint8_t *) outBuffer;
    uint8_t *lastLiteral = nullptr;
    state = BASE;
    while( remaining > 0 ) {
      uint8_t byte = *inPtr++, loop = 0;
      int run = 1;
      for( remaining--; remaining > 0 && byte == *inPtr; remaining--, run++, inPtr++ );
      do {
        loop = 0;
        switch( state ) {
          case BASE:
          case RUN: {
            if( run > 1 ) {
              state = RUN;
              RLE_OUTPUT_RUN
            } else {
              lastLiteral = outPtr;
              *outPtr++ = 0, *outPtr++ = byte;
              state = LITERAL;
            }
            break;
          }
          case LITERAL: {
            if( run > 1 ) {
              state = LITERAL_RUN;
              RLE_OUTPUT_RUN
            } else {
              if( ++(*lastLiteral) == 127 )
                state = BASE;
              *outPtr++ = byte;
            }
            break;
          }
          case LITERAL_RUN: {
            if( outPtr[-2] == 0x81 && *lastLiteral < (125)) {
              state = (((*lastLiteral) += 2) == 127) ? BASE : LITERAL;
              outPtr[-2] = outPtr[-1];
            } else
              state = RUN;
            loop = 1;
          }
        }
      } while( loop );
    }

    uint64_t length = outPtr - (&outBuffer[0]);
    if( mode == FDECOMPRESS )
      out->blockWrite(&outBuffer[0], length);
    else if( mode == FCOMPARE ) {
      for( int j = 0; j < (int) length; ++j ) {
        if( outBuffer[j] != out->getchar() && !diffFound ) {
          diffFound = pos + j + 1;
          break;
        }
      }
    }
    pos += length;
  } while( !in->eof() && !diffFound );
  return pos;
}

#endif //PAQ8PX_RLE_HPP
