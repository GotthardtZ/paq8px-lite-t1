#ifndef PAQ8PX_AUDIO16BITMODEL_HPP
#define PAQ8PX_AUDIO16BITMODEL_HPP

#include "AudioModel.hpp"
#include "../SmallStationaryContextMap.hpp"
#include "../OLS.hpp"
#include "../LMS.hpp"
#include <cstdint>
#include "../utils.hpp"

#ifdef USE_AUDIOMODEL

class Audio16BitModel : AudioModel {
private:
    static constexpr int nOLS = 8;
    static constexpr int nLMS = 3;
    static constexpr int nSSM = nOLS + nLMS + 3;
    static constexpr int nCtx = 4;
    SmallStationaryContextMap sMap1B[nSSM][nCtx];
    OLS<double, short> ols[nOLS][2] {{{128, 24, 0.9975}, {128, 24, 0.9975}},
                                     {{90,  30, 0.997},  {90,  30, 0.997}},
                                     {{90,  31, 0.996},  {90,  31, 0.996}},
                                     {{90,  32, 0.995},  {90,  32, 0.995}},
                                     {{90,  33, 0.995},  {90,  33, 0.995}},
                                     {{90,  34, 0.9985}, {90,  34, 0.9985}},
                                     {{28,  4,  0.98},   {28,  4,  0.98}},
                                     {{32,  3,  0.992},  {32,  3,  0.992}}};
    LMS<float, short> lms[nLMS][2] {{{1280, 640, 5e-5f, 5e-5f}, {1280, 640, 5e-5f, 5e-5f}},
                                    {{640,  64,  7e-5f, 1e-5f}, {640,  64,  7e-5f, 1e-5f}},
                                    {{2450, 8,   2e-5f, 2e-6f}, {2450, 8,   2e-5f, 2e-6f}}};
    int prd[nSSM][2][2] {0}, residuals[nSSM][2] {0};
    int stereo = 0, ch = 0, lsb = 0;
    uint32_t mask = 0, errLog = 0, mxCtx = 0;
    short sample = 0;

public:
    static constexpr int MIXERINPUTS = nCtx * nSSM * SmallStationaryContextMap::MIXERINPUTS;
    static constexpr int MIXERCONTEXTS = 8192 + 4096 + 2560 + 256 + 20; //15124
    static constexpr int MIXERCONTEXTSETS = 5;
    Audio16BitModel(ModelStats *st);
    void setParam(int info);
    void mix(Mixer &m);
};

#endif // USE_AUDIOMODEL
#endif //PAQ8PX_AUDIO16BITMODEL_HPP
