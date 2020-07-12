#ifndef PAQ8PX_TRANSFORMOPTIONS_HPP
#define PAQ8PX_TRANSFORMOPTIONS_HPP

struct TransformOptions
{
  bool skipRgb = false;
  bool useZlibBrute = false;
  TransformOptions(const Shared * const shared) {
    skipRgb = (shared->options & OPTION_SKIPRGB) != 0u;
    useZlibBrute = (shared->options & OPTION_BRUTE) != 0;
  }
};

#endif