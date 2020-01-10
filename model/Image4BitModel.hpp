#ifndef PAQ8PX_IMAGE4BITMODEL_HPP
#define PAQ8PX_IMAGE4BITMODEL_HPP

#include "../Shared.hpp"
#include "../RingBuffer.hpp"
#include "../Random.hpp"
#include "../StateMap.hpp"
#include "../HashTable.hpp"
#include "../Mixer.hpp"
#include "../Hash.hpp"
#include "../Ilog.hpp"

/**
 * Model for 4-bit image data
 */
class Image4BitModel {
private:
    static constexpr int S = 14; //number of contexts
    Shared* shared = Shared::getInstance();
    Random rnd;
    HashTable<16> t;
    StateMap sm;
    StateMap map;
    uint8_t *cp[S] {}; // context pointers
    uint8_t WW = 0, W = 0, NWW = 0, NW = 0, N = 0, NE = 0, NEE = 0, NNWW = 0, NNW = 0, NN = 0, NNE = 0, NNEE = 0;
    int w = 0, col = 0, line = 0, run = 0, prevColor = 0, px = 0;

public:
    static constexpr int MIXERINPUTS = (S * 3 + 1);
    static constexpr int MIXERCONTEXTS = 256 + 512 + 512 + 1024 + 16 + 1; //2321
    static constexpr int MIXERCONTEXTSETS = 6;
    Image4BitModel(uint64_t size);
    void setParam(int info0);
    void mix(Mixer &m);
};

#endif //PAQ8PX_IMAGE4BITMODEL_HPP
