#ifndef PAQ8PX_FILTERS_HPP
#define PAQ8PX_FILTERS_HPP

#include "../Array.hpp"
#include "../Encoder.hpp"
#include "../file/File.hpp"
#include "../file/FileDisk.hpp"
#include "../Utils.hpp"
#include <cctype>
#include <cstdint>
#include <cstring>



//////////////////// Compress, Decompress ////////////////////////////

typedef enum {
  FDECOMPRESS, FCOMPARE
} FMode;

static void compressfile(const Shared* const shared, const char *filename, uint64_t fileSize, Encoder &en, bool verbose) {

  uint64_t start = en.size();
  FileDisk in;
  in.open(filename, true);

  float p1 = 0.0f;
  float p2 = 1.0f;
  const float pscale = fileSize != 0 ? (p2 - p1) / fileSize : 0;
  p2 = p1 + pscale * fileSize;
  en.setStatusRange(p1, p2);

  fprintf(stderr, "Compressing... ");
  for (uint64_t j = 0; j < fileSize; ++j) {
    if ((j & 0xfffff) == 0) {
      en.printStatus(j, fileSize);
    }
    en.compressByte(&en.predictorMain, in.getchar());
  }
  fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

  p1 = p2;

  in.close();
}

static auto decompressRecursive(File *out, uint64_t blockSize, Encoder &en, FMode mode) -> uint64_t {
  for( uint64_t j = 0; j < blockSize; ++j ) {
    if((j & 0xfffff) == 0u ) {
      en.printStatus();
    }
    if( mode == FDECOMPRESS ) {
      out->putChar(en.decompressByte(&en.predictorMain));
    } else { //compare
      if( en.decompressByte(&en.predictorMain) != out->getchar()) {
        return j+1;
      }
    }
  }
  return 0;
}

// Decompress or compare a file
static void decompressFile(const Shared *const shared, const char *filename, FMode fMode, Encoder &en, uint64_t fileSize) {

  FileDisk f;
  if( fMode == FCOMPARE ) {
    f.open(filename, true);
    printf("Comparing");
  } else { //mode==FDECOMPRESS;
    f.create(filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, fileSize);

  // Decompress/Compare
  uint64_t r = decompressRecursive(&f, fileSize, en, fMode);
  if( fMode == FCOMPARE && (r == 0u) && f.getchar() != EOF) {
    printf("file is longer\n");
  } else if( fMode == FCOMPARE && (r != 0u)) {
    printf("differ at %" PRIu64 "\n", r - 1);
  } else if( fMode == FCOMPARE ) {
    printf("identical\n");
  } else {
    printf("done   \n");
  }
  f.close();
}

#endif //PAQ8PX_FILTERS_HPP
