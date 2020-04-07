#include "AudioModel.hpp"

AudioModel::AudioModel(ModelStats *st) : stats(st) {}

auto AudioModel::s2(int i) -> int {
  INJECT_SHARED_buf
  return int(short(buf(i) + 256 * buf(i - 1)));
}

auto AudioModel::t2(int i) -> int {
  INJECT_SHARED_buf
  return int(short(buf(i - 1) + 256 * buf(i)));
}

auto AudioModel::x1(int i) -> int {
  INJECT_SHARED_buf
  switch( wMode ) {
    case 0:
      return buf(i) - 128;
    case 1:
      return buf(i << 1U) - 128;
    case 2:
      return s2(i << 1U);
    case 3:
      return s2(i << 2U);
    case 4:
      return (buf(i) ^ 128U) - 128;
    case 5:
      return (buf(i << 1U) ^ 128) - 128;
    case 6:
      return t2(i << 1U);
    case 7:
      return t2(i << 2U);
    default:
      return 0;
  }
}

auto AudioModel::x2(int i) -> int {
  INJECT_SHARED_buf
  switch( wMode ) {
    case 0:
      return buf(i + s) - 128;
    case 1:
      return buf((i << 1U) - 1) - 128;
    case 2:
      return s2((i + s) << 1U);
    case 3:
      return s2((i << 2U) - 2);
    case 4:
      return (buf(i + s) ^ 128) - 128;
    case 5:
      return (buf((i << 1U) - 1) ^ 128) - 128;
    case 6:
      return t2((i + s) << 1U);
    case 7:
      return t2((i << 2U) - 2);
    default:
      return 0;
  }
}

auto AudioModel::signedClip8(const int i) -> int {
  return max(-128, min(127, i));
}

auto AudioModel::signedClip16(const int i) -> int {
  return max(-32768, min(32767, i));
}
