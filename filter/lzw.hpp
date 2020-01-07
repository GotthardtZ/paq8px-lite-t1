#ifndef PAQ8PX_LZW_HPP
#define PAQ8PX_LZW_HPP

#include "Filter.hpp"

struct LZWentry {
    short prefix;
    short suffix;
};

#define LZW_RESET_CODE 256
#define LZW_EOF_CODE 257

class LZWDictionary {
private:
    static constexpr int hashSize = 9221;
    LZWentry dictionary[4096] {};
    short table[hashSize] {};
    uint8_t buffer[4096] {};

public:
    int index;

    LZWDictionary() : index(0) { reset(); }

    void reset() {
      memset(&dictionary, 0xFF, sizeof(dictionary));
      memset(&table, 0xFF, sizeof(table));
      for( int i = 0; i < 256; i++ ) {
        table[-findEntry(-1, i) - 1] = (short) i;
        dictionary[i].suffix = i;
      }
      index = 258; //2 extra codes, one for resetting the dictionary and one for signaling EOF
    }

    int findEntry(const int prefix, const int suffix) {
      int i = finalize64(hash(prefix, suffix), 13);
      int offset = (i > 0) ? hashSize - i : 1;
      while( true ) {
        if( table[i] < 0 ) //free slot?
          return -i - 1;
        else if( dictionary[table[i]].prefix == prefix && dictionary[table[i]].suffix == suffix ) //is it the entry we want?
          return table[i];
        i -= offset;
        if( i < 0 )
          i += hashSize;
      }
    }

    void addEntry(const int prefix, const int suffix, const int offset = -1) {
      if( prefix == -1 || prefix >= index || index > 4095 || offset >= 0 )
        return;
      dictionary[index].prefix = prefix;
      dictionary[index].suffix = suffix;
      table[-offset - 1] = index;
      index += (index < 4096);
    }

    int dumpEntry(File *f, int code) {
      int n = 4095;
      while( code > 256 && n >= 0 ) {
        buffer[n] = uint8_t(dictionary[code].suffix);
        n--;
        code = dictionary[code].prefix;
      }
      buffer[n] = uint8_t(code);
      f->blockWrite(&buffer[n], 4096 - n);
      return code;
    }
};

class LZWFilter : Filter {
    void encode(File *in, File *out, uint64_t size, int info, int &headerSize) override {
      LZWDictionary dic;
      int parent = -1, code = 0, buffer = 0, bitsPerCode = 9, bitsUsed = 0;
      bool done = false;
      while( !done ) {
        buffer = in->getchar();
        if( buffer < 0 ) {
          return;// 0;
        }
        for( int j = 0; j < 8; j++ ) {
          code += code + ((buffer >> (7 - j)) & 1), bitsUsed++;
          if( bitsUsed >= bitsPerCode ) {
            if( code == LZW_EOF_CODE ) {
              done = true;
              break;
            } else if( code == LZW_RESET_CODE ) {
              dic.reset();
              parent = -1;
              bitsPerCode = 9;
            } else {
              if( code < dic.index ) {
                if( parent != -1 )
                  dic.addEntry(parent, dic.dumpEntry(out, code));
                else
                  out->putChar(code);
              } else if( code == dic.index ) {
                int a = dic.dumpEntry(out, parent);
                out->putChar(a);
                dic.addEntry(parent, a);
              } else
                return;// 0;
              parent = code;
            }
            bitsUsed = 0;
            code = 0;
            if((1U << bitsPerCode) == dic.index + 1 && dic.index < 4096 )
              bitsPerCode++;
          }
        }
      }
      return;// 1;
    }

    uint64_t decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) override {
      return 0;
    }

    void setEncoder(Encoder &en) override {

    }

};
int encodeLzw(File *in, File *out, uint64_t size, int &headerSize) {
  LZWDictionary dic;
  int parent = -1, code = 0, buffer = 0, bitsPerCode = 9, bitsUsed = 0;
  bool done = false;
  while( !done ) {
    buffer = in->getchar();
    if( buffer < 0 ) {
      return 0;
    }
    for( int j = 0; j < 8; j++ ) {
      code += code + ((buffer >> (7 - j)) & 1U), bitsUsed++;
      if( bitsUsed >= bitsPerCode ) {
        if( code == LZW_EOF_CODE ) {
          done = true;
          break;
        } else if( code == LZW_RESET_CODE ) {
          dic.reset();
          parent = -1;
          bitsPerCode = 9;
        } else {
          if( code < dic.index ) {
            if( parent != -1 )
              dic.addEntry(parent, dic.dumpEntry(out, code));
            else
              out->putChar(code);
          } else if( code == dic.index ) {
            int a = dic.dumpEntry(out, parent);
            out->putChar(a);
            dic.addEntry(parent, a);
          } else
            return 0;
          parent = code;
        }
        bitsUsed = 0;
        code = 0;
        if((1 << bitsPerCode) == dic.index + 1 && dic.index < 4096 )
          bitsPerCode++;
      }
    }
  }
  return 1;
}

inline void writeCode(File *f, const FMode mode, int *buffer, uint64_t *pos, int *bitsUsed, const int bitsPerCode, const int code,
                      uint64_t *diffFound) {
  *buffer <<= bitsPerCode;
  *buffer |= code;
  (*bitsUsed) += bitsPerCode;
  while((*bitsUsed) > 7 ) {
    const uint8_t b = *buffer >> (*bitsUsed -= 8);
    (*pos)++;
    if( mode == FDECOMPRESS )
      f->putChar(b);
    else if( mode == FCOMPARE && b != f->getchar())
      *diffFound = *pos;
  }
}

uint64_t decodeLzw(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  LZWDictionary dic;
  uint64_t pos = 0;
  int parent = -1, code = 0, buffer = 0, bitsPerCode = 9, bitsUsed = 0;
  writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_RESET_CODE, &diffFound);
  while((code = in->getchar()) >= 0 && diffFound == 0 ) {
    int index = dic.findEntry(parent, code);
    if( index < 0 ) { // entry not found
      writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, parent, &diffFound);
      if( dic.index > 4092 ) {
        writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_RESET_CODE, &diffFound);
        dic.reset();
        bitsPerCode = 9;
      } else {
        dic.addEntry(parent, code, index);
        if( dic.index >= (1U << bitsPerCode))
          bitsPerCode++;
      }
      parent = code;
    } else
      parent = index;
  }
  if( parent >= 0 )
    writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, parent, &diffFound);
  writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_EOF_CODE, &diffFound);
  if( bitsUsed > 0 ) { // flush buffer
    pos++;
    if( mode == FDECOMPRESS )
      out->putChar(uint8_t(buffer));
    else if( mode == FCOMPARE && uint8_t(buffer) != out->getchar())
      diffFound = pos;
  }
  return pos;
}

#endif //PAQ8PX_LZW_HPP
