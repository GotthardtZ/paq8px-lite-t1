#include "Encoder.hpp"
#include <math.h>

Encoder::Encoder(Shared* const sh, Mode m, File *f) : shared(sh), ari(f), mode(m), archive(f), alt(nullptr), predictorMain(sh) {
  if( mode == DECOMPRESS ) {
    uint64_t start = size();
    archive->setEnd();
    uint64_t end = size();
    setStatusRange(0.0, static_cast<float>(end));
    archive->setpos(start);
    ari.prefetch();
  }
}

auto Encoder::getMode() const -> Mode {
  return mode; 
}

auto Encoder::size() const -> uint64_t {
  return archive->curPos(); 
}

void Encoder::flush() {
  ari.flush();
}

void Encoder::setFile(File *f) { alt = f; }

void Encoder::compressByte(Predictor *predictor, uint8_t c) {
    for( int i = 7; i >= 0; --i ) {
      uint32_t p = predictor->p();
      int y = (c >> i) & 1;
      ari.encodeBit(p, y);
      updateModels(predictor, p, y);
      
    }
    assert(shared->State.c1 == c);
}

uint8_t Encoder::decompressByte(Predictor *predictor) {
  for( int i = 0; i < 8; ++i ) {
    int p = predictor->p();
    int y = ari.decodeBit(p);
    updateModels(predictor, p, y);
  }
  return shared->State.c1;
}

void Encoder::updateModels(Predictor* predictor, uint32_t p, int y) {
  bool isMissed = ((p >> (16 - 1)) != y);
  shared->update(y, isMissed);
  predictor->Update();
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
