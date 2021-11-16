#include "FileDisk.hpp"
#include "../SystemDefines.hpp"

FileDisk::FileDisk() { file = nullptr; }

FileDisk::~FileDisk() { close(); }

auto FileDisk::open(const char *filename, bool mustSucceed) -> bool {
  assert(file == nullptr);
  file = openFile(filename, READ);
  const bool success = (file != nullptr);
  if( !success && mustSucceed ) {
    printf("Unable to open file %s (%s)", filename, strerror(errno));
    quit();
  }
  return success;
}

void FileDisk::create(const char *filename) {
  assert(file == nullptr);
  makeDirectories(filename);
  file = openFile(filename, WRITE);
  if( file == nullptr ) {
    printf("Unable to create file %s (%s)", filename, strerror(errno));
    quit();
  }
}

void FileDisk::close() {
  if( file != nullptr ) {
    fclose(file);
  }
  file = nullptr;
}

auto FileDisk::getchar() -> int { return fgetc(file); }

void FileDisk::putChar(uint8_t c) { fputc(c, file); }

void FileDisk::setpos(uint64_t newPos) { fseeko(file, newPos, SEEK_SET); }

void FileDisk::setEnd() { fseeko(file, 0, SEEK_END); }

auto FileDisk::curPos() -> uint64_t { return ftello(file); }

auto FileDisk::eof() -> bool { return feof(file) != 0; }