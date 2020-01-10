#ifndef PAQ8PX_NESTMODEL_HPP
#define PAQ8PX_NESTMODEL_HPP

#include "../Shared.hpp"
#include "../ContextMap.hpp"

class NestModel {
private:
    static constexpr int nCM = 12;
    Shared *shared = Shared::getInstance();
    int ic = 0, bc = 0, pc = 0, vc = 0, qc = 0, lvc = 0, wc = 0, ac = 0, ec = 0, uc = 0, sense1 = 0, sense2 = 0, w = 0;
    ContextMap cm;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 60
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    NestModel(uint64_t size);
    void mix(Mixer &m);
};

#endif //PAQ8PX_NESTMODEL_HPP
