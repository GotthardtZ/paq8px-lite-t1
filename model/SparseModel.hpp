#ifndef PAQ8PX_SPARSEMODEL_HPP
#define PAQ8PX_SPARSEMODEL_HPP

#include <cassert>
#include <cstdint>
#include "../Shared.hpp"
#include "../Mixer.hpp"
#include "../ContextMap.hpp"

/**
 * Model order 1-2-3 contexts with gaps.
 */
class SparseModel {
private:
    static constexpr int nCM = 38; //17+3*7
    const Shared *const shared;
    ContextMap cm;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 190
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    SparseModel(const Shared *sh, uint64_t size);
    void mix(Mixer &m);
};

#endif //PAQ8PX_SPARSEMODEL_HPP
