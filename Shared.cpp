#include "Shared.hpp"

Shared::Shared() {
  toScreen = !isOutputRedirected();
}

void Shared::init(uint8_t level, uint32_t bufMem) {
  this->level = level;
  mem = UINT64_C(65536) << level;
  if (bufMem == 0) //to auto size
    bufMem = static_cast<uint32_t>(16); 
  assert(isPowerOf2(bufMem));
  buf.setSize(bufMem);
  toScreen = !isOutputRedirected();
}

void Shared::update(int y, bool isMissed) {
  State.y = y;
  State.c0 += State.c0 + y;
  State.bitPosition = (State.bitPosition + 1U) & 7U;
  if(State.bitPosition == 0 ) {
    State.c1 = State.c0;
    buf.add(State.c1);
    State.c4 = (State.c4 << 8U) | State.c0;
    State.c0 = 1;
  }
  
  State.misses = State.misses << 1 | isMissed;
}

void Shared::reset() {
  buf.reset();
  memset(&State, 0, sizeof(State));
  State.c0 = 1;
}

auto Shared::isOutputRedirected() -> bool {
#ifdef WINDOWS
  DWORD FileType = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
  return (FileType == FILE_TYPE_PIPE) || (FileType == FILE_TYPE_DISK);
#endif
#ifdef UNIX
  return isatty(fileno(stdout)) == 0;
#endif
}
