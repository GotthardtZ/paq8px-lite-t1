#ifndef PAQ8PX_RLE_HPP
#define PAQ8PX_RLE_HPP

#include "Filter.hpp"

#define RLE_OUTPUT_RUN \
  { \
    while (run > 128) { \
      *outPtr++ = 0xFF, *outPtr++ = byte; \
      run -= 128; \
    } \
    *outPtr++ = (uint8_t)(0x80U | (run - 1)), *outPtr++ = byte; \
  }

class RleFilter : Filter {
private:
    enum {
        BASE, LITERAL, RUN, LITERAL_RUN
    } state;

    void handleRun(uint8_t byte, uint8_t *&outPtr, uint8_t *&lastLiteral, int &run) {
      if( run > 1 ) {
        state = RUN;
        RLE_OUTPUT_RUN
      } else {
        lastLiteral = outPtr;
        *outPtr++ = 0, *outPtr++ = byte;
        state = LITERAL;
      }
    }

    void handleLiteral(uint8_t byte, uint8_t *&outPtr, uint8_t *lastLiteral, int &run) {
      if( run > 1 ) {
        state = LITERAL_RUN;
        RLE_OUTPUT_RUN
      } else {
        if( ++(*lastLiteral) == 127 )
          state = BASE;
        *outPtr++ = byte;
      }
    }

    uint8_t handleLiteralRun(uint8_t *outPtr, uint8_t *lastLiteral) {
      uint8_t loop;
      if( outPtr[-2] == 0x81 && *lastLiteral < (125)) {
        state = (((*lastLiteral) += 2) == 127) ? BASE : LITERAL;
        outPtr[-2] = outPtr[-1];
      } else
        state = RUN;
      loop = 1;
      return loop;
    }

public:
    void encode(File *in, File *out, uint64_t size, int info, int &headerSize) override {
      uint8_t b, c = in->getchar();
      int i = 1, maxBlockSize = info & 0xFFFFFFU;
      out->putVLI(maxBlockSize);
      headerSize = VLICost(maxBlockSize);
      while( i < (int) size ) {
        b = in->getchar(), i++;
        if( c == 0x80 ) {
          c = b;
          continue;
        } else if( c > 0x7F ) {
          for( int j = 0; j <= (c & 0x7FU); j++ )
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

    uint64_t decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) override {
      uint8_t inBuffer[0x10000] = {0};
      uint8_t outBuffer[0x10200] = {0};
      uint64_t pos = 0;
      int maxBlockSize = (int) in->getVLI();

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
                handleRun(byte, outPtr, lastLiteral, run);
                break;
              }
              case LITERAL: {
                handleLiteral(byte, outPtr, lastLiteral, run);
                break;
              }
              case LITERAL_RUN: {
                loop = handleLiteralRun(outPtr, lastLiteral);
              }
            }
          } while( loop );
        }

        uint64_t length = outPtr - (&outBuffer[0]);
        if( fMode == FDECOMPRESS )
          out->blockWrite(&outBuffer[0], length);
        else if( fMode == FCOMPARE ) {
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

    void setEncoder(Encoder &en) override {}
};

#endif //PAQ8PX_RLE_HPP
