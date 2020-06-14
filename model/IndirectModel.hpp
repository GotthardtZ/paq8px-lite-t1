#ifndef PAQ8PX_INDIRECTMODEL_HPP
#define PAQ8PX_INDIRECTMODEL_HPP

#include "../ContextMap.hpp"
#include "../IndirectContext.hpp"
#include "../Mixer.hpp"
#include "../RingBuffer.hpp"
#include "../Shared.hpp"
#include <cctype>

/**
 * The context is a byte string history that occurs within a 1 or 2 byte context.
 */
class IndirectModel {
private:
    static constexpr int nCM = 15;
    const Shared * const shared;
    ContextMap cm;
    Array<uint32_t> t1 {256};
    Array<uint16_t> t2 {0x10000};
    Array<uint16_t> t3 {0x8000};
    Array<uint16_t> t4 {0x8000};
    IndirectContext<uint32_t> iCtx {16, 8};

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 75
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
    explicit IndirectModel(const Shared* const sh, uint64_t size);
    void mix(Mixer &m);
};

#endif //PAQ8PX_INDIRECTMODEL_HPP
