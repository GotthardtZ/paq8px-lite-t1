#ifndef PAQ8PX_ENDIANNESS16B_HPP
#define PAQ8PX_ENDIANNESS16B_HPP

#include "../Encoder.hpp"
#include "../file/File.hpp"
#include "Filter.hpp"
#include <cstdint>

class EndiannessFilter : public Filter {
public:
    void encode(File *in, File *out, uint64_t size, int info, int &headerSize) override {
      for( uint64_t i = 0, l = size >> 1U; i < l; i++ ) {
        uint8_t b = in->getchar();
        out->putChar(in->getchar());
        out->putChar(b);
      }
      if((size & 1U) > 0 )
        out->putChar(in->getchar());
    }

    uint64_t decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) override {
      for( uint64_t i = 0, l = size >> 1U; i < l; i++ ) {
        uint8_t b1 = encoder->decompress();
        uint8_t b2 = encoder->decompress();
        if( fMode == FDECOMPRESS ) {
          out->putChar(b2);
          out->putChar(b1);
        } else if( fMode == FCOMPARE ) {
          bool ok = out->getchar() == b2;
          ok &= out->getchar() == b1;
          if( !ok && (diffFound == 0u)) {
            diffFound = size - i * 2;
            break;
          }
        }
        if( fMode == FDECOMPRESS && ((i & 0x7FFU) == 0u))
          encoder->printStatus();
      }
      if((diffFound == 0u) && (size & 1U) > 0 ) {
        if( fMode == FDECOMPRESS )
          out->putChar(encoder->decompress());
        else if( fMode == FCOMPARE ) {
          if( out->getchar() != encoder->decompress())
            diffFound = size - 1;
        }
      }
      return size;
    }

};

#endif //PAQ8PX_ENDIANNESS16B_HPP
