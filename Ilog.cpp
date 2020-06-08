#include "Ilog.hpp"

auto Ilog::log(uint16_t x) const -> int { return static_cast<int>(t[x]); }

Ilog *Ilog::mPInstance = nullptr;

auto Ilog::getInstance() -> Ilog * {
  if( mPInstance == nullptr ) {   // Only allow one getInstance of class to be generated.
    mPInstance = new Ilog;
  }

  return mPInstance;
}

Ilog *ilog = Ilog::getInstance();

auto llog(uint32_t x) -> int {
  if( x >= 0x1000000 ) {
    return 256 + ilog->log(uint16_t(x >> 16U));
  }
  if( x >= 0x10000 ) {
    return 128 + ilog->log(uint16_t(x >> 8U));
  }

  return ilog->log(uint16_t(x));
}
