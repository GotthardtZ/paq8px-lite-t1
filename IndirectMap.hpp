#ifndef PAQ8PX_INDIRECTMAP_HPP
#define PAQ8PX_INDIRECTMAP_HPP

#include "IPredictor.hpp"
#include <cstdint>
#include <cassert>
#include "StateMap.hpp"
#include "UpdateBroadcaster.hpp"
#include "Shared.hpp"
#include "Mixer.hpp"
#include "Hash.hpp"

class IndirectMap : IPredictor {
public:
    static constexpr int MIXERINPUTS = 2;

private:
    Shared *shared = Shared::getInstance();
    Random rnd;
    Array<uint8_t> data;
    StateMap sm;
    const uint32_t mask;
    const uint32_t maskBits;
    const uint32_t stride;
    const uint32_t bTotal;
    uint32_t b {};
    uint32_t bCount {};
    uint32_t context {};
    uint8_t *cp;
    int scale;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    IndirectMap(int bitsOfContext, int inputBits, int scale, int limit);
    void setDirect(uint32_t ctx);
    void set(uint64_t ctx);
    void update() override;
    void setScale(int Scale);
    void mix(Mixer &m);
};

#endif //PAQ8PX_INDIRECTMAP_HPP
