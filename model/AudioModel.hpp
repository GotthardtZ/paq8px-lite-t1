#ifndef PAQ8PX_AUDIOMODEL_HPP
#define PAQ8PX_AUDIOMODEL_HPP

#include "../ModelStats.hpp"
#include "../Shared.hpp"
#include <cstdint>

/**
 * Model a 16/8-bit stereo/mono uncompressed .wav file.
 * Based on 'An asymptotically Optimal Predictor for Stereo Lossless Audio Compression' by Florin Ghido.
 * Base class for common functions of the 16-bit and 8-bit audio models.
 */
class AudioModel {
protected:
    Shared *shared = Shared::getInstance();
    ModelStats *stats;
    int s = 0;
    int wMode = 0;
    explicit AudioModel(ModelStats *st);
    int s2(int i);
    int t2(int i);
    int x1(int i);
    int x2(int i);
    static int signedClip8(int i);
    static int signedClip16(int i);
};

#endif //PAQ8PX_AUDIOMODEL_HPP
