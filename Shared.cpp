#include "Shared.hpp"

Shared *Shared::mPInstance = nullptr;

auto Shared::getInstance() -> Shared * {
  if( mPInstance == nullptr ) {
    mPInstance = new Shared;
    mPInstance->toScreen = !Shared::isOutputDirected();
  }

  return mPInstance;
}

void Shared::update() {
  c0 += c0 + y;
  bitPosition = (bitPosition + 1U) & 7U;
  if( bitPosition == 0 ) {
    c1 = c0;
    buf.add(c1);
    c8 = (c8 << 8U) | (c4 >> 24U);
    c4 = (c4 << 8U) | c0;
    c0 = 1;
  }
}

void Shared::reset() {
  buf.reset();
  y = 0;
  c0 = 1;
  c1 = 0;
  bitPosition = 0;
  c4 = 0;
  c8 = 0;
}

/*
 relationship between compression level, shared->mem and buf memory use ( shared->mem * 8 )

 level   shared->mem    buf memory use
 -----   -----------    --------------
   1      0.125 MB              1 MB
   2      0.25	MB              2 MB
   3      0.5   MB              4 MB
   4      1.0   MB              8 MB
   5      2.0   MB             16 MB
   6      4.0   MB             32 MB
   7      8.0   MB             64 MB
   8     16.0   MB            128 MB
   9     32.0   MB            256 MB
  10     64.0   MB            512 MB
  11    128.0   MB           1024 MB
  12    256.0   MB           1024 MB

*/

void Shared::setLevel(uint8_t level) {
  this->level = level;
  mem = 65536ULL << level;
  buf.setSize(min(mem * 8, 1ULL << 30)); /**< no reason to go over 1 GB */
}

auto Shared::isOutputDirected() -> bool {
#ifdef WINDOWS
  DWORD FileType = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
      return (FileType == FILE_TYPE_PIPE) || (FileType == FILE_TYPE_DISK);
#endif
#ifdef UNIX
  return isatty(fileno(stdout)) == 0;
#endif
}
