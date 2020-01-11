#ifndef PAQ8PX_ENCODER_HPP
#define PAQ8PX_ENCODER_HPP

#include <cstdint>
#include <cstdio>
#include <cassert>
#include "Predictor.hpp"

typedef enum {
    COMPRESS, DECOMPRESS
} Mode;

/**
 * An Encoder does arithmetic encoding.
 * If level (global) is 0, then data is stored without arithmetic coding.
 * TODO: Split into separate declarations / definitions
 */
class Encoder {
private:
    Predictor predictor;
    const Mode mode; // Compress or decompress?
    File *archive; // Compressed data file
    uint32_t x1, x2; // Range, initially [0, 1), scaled by 2^32
    uint32_t x; // Decompress mode: last 4 input bytes of archive
    File *alt; // decompress() source in COMPRESS mode
    float p1 {}, p2 {}; // percentages for progress indicator: 0.0 .. 1.0

    /**
     * code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
     * code() in DECOMPRESS mode returns the next decompressed bit from file f.
     * Global y is set to the last bit coded or decoded by code().
     * @param i the bit to be compressed
     * @return
     */
    int code(int i = 0) {
      int p = predictor.p();
      if( p == 0 )
        p++;
      assert(p > 0 && p < 4096);
      uint32_t xMid = x1 + ((x2 - x1) >> 12U) * p + (((x2 - x1) & 0xfffu) * p >> 12);
      assert(xMid >= x1 && xMid < x2);
      uint8_t y = (mode == DECOMPRESS) ? x <= xMid : i;
      y ? (x2 = xMid) : (x1 = xMid + 1);
      while(((x1 ^ x2) & 0xff000000) == 0 ) { // pass equal leading bytes of range
        if( mode == COMPRESS )
          archive->putChar(x2 >> 24U);
        x1 <<= 8U;
        x2 = (x2 << 8U) + 255;
        if( mode == DECOMPRESS )
          x = (x << 8U) + (archive->getchar() & 255U); // EOF is OK
      }
      predictor.update(y);
      return y;
    }

public:
    /**
     * Encoder(COMPRESS, f) creates encoder for compression to archive f, which
     * must be open past any header for writing in binary mode.
     * Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
     * which must be open past any header for reading in binary mode.
     * @param m the mode to operate in
     * @param f the file to read from or write to
     */
    Encoder(Mode m, File *f, uint32_t level);
    [[nodiscard]] Mode getMode() const;
    /**
     * size() returns current length of archive
     * @return length of archive so far
     */
    [[nodiscard]] uint64_t size() const;
    /**
     * flush() should be called exactly once after compression is done and
     * before closing f. It does nothing in DECOMPRESS mode.
     */
    void flush();
    /**
     * setFile(f) sets alternate source to File *f for decompress() in COMPRESS
     * mode (for testing transforms).
     * @param f
     */
    void setFile(File *f);
    /**
     * compress(c) in COMPRESS mode compresses one byte.
     * @param c the byte to be compressed
     */
    void compress(int c);
    /**
     * decompress() in DECOMPRESS mode decompresses and returns one byte.
     * @return the decompressed byte
     */
    int decompress();
    void encodeBlockSize(uint64_t blockSize);
    uint64_t decodeBlockSize();
    void setStatusRange(float perc1, float perc2);
    void printStatus(uint64_t n, uint64_t size);
    void printStatus();
};

Encoder::Encoder(Mode m, File *f, uint32_t level) : predictor(level), mode(m), archive(f), x1(0), x2(0xffffffff), x(0), alt(nullptr) {
  if( mode == DECOMPRESS ) {
    uint64_t start = size();
    archive->setEnd();
    uint64_t end = size();
    if( end >= (UINT64_C(1) << 31))
      quit("Large archives not yet supported.");
    setStatusRange(0.0, (float) end);
    archive->setpos(start);
  }
  if( level > 0 && mode == DECOMPRESS ) { // x = first 4 bytes of archive
    for( int i = 0; i < 4; ++i )
      x = (x << 8U) + (archive->getchar() & 255U);
  }
}

Mode Encoder::getMode() const { return mode; }

uint64_t Encoder::size() const { return archive->curPos(); }

void Encoder::flush() {
  if( mode == COMPRESS && level > 0 )
    archive->putChar(x1 >> 24U); // Flush first unequal byte of range
}

void Encoder::setFile(File *f) { alt = f; }

void Encoder::compress(int c) {
  assert(mode == COMPRESS);
  if( level == 0 )
    archive->putChar(c);
  else
    for( int i = 7; i >= 0; --i )
      code((c >> i) & 1);
}

int Encoder::decompress() {
  if( mode == COMPRESS ) {
    assert(alt);
    return alt->getchar();
  } else if( level == 0 )
    return archive->getchar();
  else {
    int c = 0;
    for( int i = 0; i < 8; ++i )
      c += c + code();
    return c;
  }
}

void Encoder::encodeBlockSize(uint64_t blockSize) {
  //TODO: Large file support
  while( blockSize > 0x7FU ) {
    compress(0x80U | (blockSize & 0x7FU));
    blockSize >>= 7U;
  }
  compress(uint8_t(blockSize));
}

uint64_t Encoder::decodeBlockSize() {
  //TODO: Large file support
  uint64_t blockSize = 0;
  uint8_t b = 0;
  int i = 0;
  do {
    b = decompress();
    blockSize |= uint64_t((b & 0x7FU) << i);
    i += 7;
  } while((b >> 7U) > 0 );
  return blockSize;
}

void Encoder::setStatusRange(float perc1, float perc2) {
  p1 = perc1;
  p2 = perc2;
}

void Encoder::printStatus(uint64_t n, uint64_t size) {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", (p1 + (p2 - p1) * n / (size + 1)) * 100);
  fflush(stderr);
}

void Encoder::printStatus() {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", float(size()) / (p2 + 1) * 100);
  fflush(stderr);
}

#endif //PAQ8PX_ENCODER_HPP
