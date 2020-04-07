#ifndef PAQ8PX_SPARSEMODEL_HPP
#define PAQ8PX_SPARSEMODEL_HPP

#include "../ContextMap.hpp"
#include "../Mixer.hpp"
#include "../Shared.hpp"
#include <cassert>
#include <cstdint>

/**
 * Model order 1-2-3 contexts with gaps.
 */
class SparseModel {
private:
    static constexpr int nCM = 38; //17+3*7
    Shared *shared = Shared::getInstance();
    ContextMap cm;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 190
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    explicit SparseModel(uint64_t size);
    void mix(Mixer &m);
};

#endif //PAQ8PX_SPARSEMODEL_HPP
