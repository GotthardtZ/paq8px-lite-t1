#ifndef PAQ8PX_LINEARPREDICTIONMODEL_HPP
#define PAQ8PX_LINEARPREDICTIONMODEL_HPP

#include "../OLS.hpp"
#include "../SmallStationaryContextMap.hpp"
#include <cmath>

class LinearPredictionModel {
private:
    static constexpr int nOLS = 3;
    static constexpr int nSSM = nOLS + 2;
    Shared *shared = Shared::getInstance();
    SmallStationaryContextMap sMap[nSSM];
    OLS<double, uint8_t> ols[nOLS] {{32, 4, 0.995},
                                    {32, 4, 0.995},
                                    {32, 4, 0.995}};
    uint8_t prd[nSSM] {0};

public:
    static constexpr int MIXERINPUTS = nSSM * SmallStationaryContextMap::MIXERINPUTS; // 10
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    LinearPredictionModel();
    void mix(Mixer &m);
};

#endif //PAQ8PX_LINEARPREDICTIONMODEL_HPP
