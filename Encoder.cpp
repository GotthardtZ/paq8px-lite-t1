#include "Encoder.hpp"

auto Encoder::code(int i) -> int {
  int p = predictor.p();
  if( p == 0 ) {
    p++;
  }
  assert(p > 0 && p < 4096);
  uint32_t xMid = x1 + ((x2 - x1) >> 12U) * p + (((x2 - x1) & 0xfffU) * p >> 12U);
  assert(xMid >= x1 && xMid < x2);
  uint8_t y = (mode == DECOMPRESS) ? static_cast<int>(x <= xMid) : i;
  y != 0u ? (x2 = xMid) : (x1 = xMid + 1);
  while(((x1 ^ x2) & 0xff000000U) == 0 ) { // pass equal leading bytes of range
    if( mode == COMPRESS ) {
      archive->putChar(x2 >> 24U);
    }
    x1 <<= 8U;
    x2 = (x2 << 8U) + 255;
    if( mode == DECOMPRESS ) {
      x = (x << 8U) + (archive->getchar() & 255U); // EOF is OK
    }
  }
  predictor.update(y);
  return y;
}

Encoder::Encoder(Mode m, File *f) : mode(m), archive(f), x1(0), x2(0xffffffff), x(0), alt(nullptr) {
  if( mode == DECOMPRESS ) {
    uint64_t start = size();
    archive->setEnd();
    uint64_t end = size();
    if( end >= (1ULL << 31U)) {
      quit("Large archives not yet supported.");
    }
    setStatusRange(0.0, static_cast<float>(end));
    archive->setpos(start);
  }
  if( shared->level > 0 && mode == DECOMPRESS ) { // x = first 4 bytes of archive
    for( int i = 0; i < 4; ++i ) {
      x = (x << 8U) + (archive->getchar() & 255U);
    }
  }
}

auto Encoder::getMode() const -> Mode { return mode; }

auto Encoder::size() const -> uint64_t { return archive->curPos(); }

void Encoder::flush() {
  if( mode == COMPRESS && shared->level > 0 ) {
    archive->putChar(x1 >> 24U); // Flush first unequal byte of range
  }
}

void Encoder::setFile(File *f) { alt = f; }

void Encoder::compress(int c) {
  assert(mode == COMPRESS);
  if( shared->level == 0 ) {
    archive->putChar(c);
  } else {
    for( int i = 7; i >= 0; --i ) {
      code((c >> i) & 1U);
    }
  }
}

auto Encoder::decompress() -> int {
  if( mode == COMPRESS ) {
    assert(alt);
    return alt->getchar();
  }
  if( shared->level == 0 ) {
    return archive->getchar();
  } else {
    int c = 0;
    for( int i = 0; i < 8; ++i ) {
      c += c + code();
    }
    return c;
  }
}

void Encoder::encodeBlockSize(uint64_t blockSize) {
  while( blockSize > 0x7FU ) {
    compress(0x80U | (blockSize & 0x7FU));
    blockSize >>= 7U;
  }
  compress(uint8_t(blockSize));
}

auto Encoder::decodeBlockSize() -> uint64_t {
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

void Encoder::printStatus(uint64_t n, uint64_t size) const {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", (p1 + (p2 - p1) * n / (size + 1)) * 100);
  fflush(stderr);
}

void Encoder::printStatus() const {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", float(size()) / (p2 + 1) * 100);
  fflush(stderr);
}
