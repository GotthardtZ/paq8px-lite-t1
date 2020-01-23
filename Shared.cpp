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

void Shared::setLevel(uint8_t l) {
  level = l;
  mem = 65536ULL << level;
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
