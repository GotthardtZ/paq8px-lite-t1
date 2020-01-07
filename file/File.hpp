#ifndef PAQ8PX_FILE_HPP
#define PAQ8PX_FILE_HPP

#include <cerrno>
#include "../String.hpp"
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <cstring>

/**
 * This is an abstract class for all the required file operations.
 * The main purpose of these classes is to keep temporary files in RAM as mush as possible. The default behaviour is to simply
 * pass function calls to the operating system - except in case of temporary files.
 */
class File {
public:
    virtual ~File() = default;;
    virtual bool open(const char *filename, bool mustSucceed) = 0;
    virtual void create(const char *filename) = 0;
    virtual void close() = 0;
    virtual int getchar() = 0;
    virtual void putChar(uint8_t c) = 0;

    void append(const char *s) {
      for( int i = 0; s[i]; i++ )
        putChar((uint8_t) s[i]);
    }

    virtual uint64_t blockRead(uint8_t *ptr, uint64_t count) = 0;
    virtual void blockWrite(uint8_t *ptr, uint64_t count) = 0;

    uint32_t get32() { return (getchar() << 24) | (getchar() << 16) | (getchar() << 8) | (getchar()); }

    void put32(uint32_t x) {
      putChar((x >> 24) & 255);
      putChar((x >> 16) & 255);
      putChar((x >> 8) & 255);
      putChar(x & 255);
    }

    uint64_t getVLI() {
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

    void putVLI(uint64_t i) {
      while( i > 0x7F ) {
        putChar(0x80U | (i & 0x7FU));
        i >>= 7U;
      }
      putChar(uint8_t(i));
    }

    virtual void setpos(uint64_t newPos) = 0;
    virtual void setEnd() = 0;
    virtual uint64_t curPos() = 0;
    virtual bool eof() = 0;
};

#endif //PAQ8PX_FILE_HPP
