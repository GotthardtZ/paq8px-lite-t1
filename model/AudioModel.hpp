#ifndef PAQ8PX_AUDIOMODEL_HPP
#define PAQ8PX_AUDIOMODEL_HPP

#include <cstdint>
#include "../Shared.hpp"
#include "../ModelStats.hpp"

#ifdef USE_AUDIOMODEL

/**
 * Model a 16/8-bit stereo/mono uncompressed .wav file.
 * Based on 'An asymptotically Optimal Predictor for Stereo Lossless Audio Compression'
 * by Florin Ghido.
 * Base class for common functions of the 16-bit and 8-bit audio models.
 */
class AudioModel {
protected:
    Shared *shared = Shared::getInstance();
    ModelStats *stats;
    int s = 0;
    int wMode = 0;

    AudioModel(ModelStats *st) : stats(st) {}

    inline int s2(int i) {
      INJECT_SHARED_buf
      return int(short(buf(i) + 256 * buf(i - 1)));
    }

    inline int t2(int i) {
      INJECT_SHARED_buf
      return int(short(buf(i - 1) + 256 * buf(i)));
    }

    int x1(int i) {
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

    int x2(int i) {
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

    static inline int signedClip8(const int i) {
      return max(-128, min(127, i));
    }

    static inline int signedClip16(const int i) {
      return max(-32768, min(32767, i));
    }
};

#endif //USE_AUDIOMODEL

#endif //PAQ8PX_AUDIOMODEL_HPP
