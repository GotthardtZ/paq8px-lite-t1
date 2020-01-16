#ifndef PAQ8PX_CONTEXTMAP2_HPP
#define PAQ8PX_CONTEXTMAP2_HPP

//TODO: update this documentation
/**
context map for large contexts.
maps to a bit history state, a 3 mostRecentlyUsed byte history, and 1 byte RunStats.

Bit and byte histories are stored in a hash table with 64 byte buckets.
The buckets are indexed by a context ending after 0, 2 or 5 bits of the
current byte. Thus, each byte modeled results in 3 main memory accesses
per context, with all other accesses to cache.

On a byte boundary (bit 0), only 3 of the 7 bit history states are used.
Of the remaining 4 bytes, 1 byte is used as a run length (the consecutive
occurrences of the previously seen byte), 3 are used to store the last
3 distinct bytes seen in this context. The byte history is then combined
with the bit history states to provide additional states that are then
mapped to predictions.
*/

#include "IPredictor.hpp"
#include "Random.hpp"
#include "StateMap.hpp"
#include "Ilog.hpp"
#include "Hash.hpp"
#include "StateTable.hpp"
#include "Mixer.hpp"
#include "UpdateBroadcaster.hpp"
#include "Stretch.hpp"
#include "Bucket.hpp"

#define CM_USE_NONE 0U
#define CM_USE_RUN_STATS 1U
#define CM_USE_BYTE_HISTORY 2U

class ContextMap2 : IPredictor {
public:
    static constexpr int MIXERINPUTS = 4;
    static constexpr int MIXERINPUTS_RUN_STATS = 1;
    static constexpr int MIXERINPUTS_BYTE_HISTORY = 2;

private:
    Shared *shared = Shared::getInstance();
    Random rnd;
    const uint32_t c; // max number of contexts
    Array<Bucket, 64> table; // bit histories for bits 0-1, 2-4, 5-7
    // For 0-1, also contains run stats in bitState[][3] and byte history in bitState[][4..6]
    Array<uint8_t *> bitState; // c pointers to current bit history states
    Array<uint8_t *> bitState0; // First element of 7 element array containing bitState[i]
    Array<uint8_t *> byteHistory; // c pointers to run stats plus byte history, 4 bytes, [RunStats,1..3]
    Array<uint32_t> contexts; // c whole byte context hashes
    Array<uint16_t> checksums; // c whole byte context checksums
    StateMap runMap, stateMap, bhMap8B, bhMap12B;
    uint32_t index; // next context to set by set()
    const uint32_t mask;
    const int hashBits;
    uint64_t validFlags;
    int scale;
    uint32_t useWhat;
    UpdateBroadcaster *updater = UpdateBroadcaster::getInstance();

public:
    int order = 0; // is set after mix()
    /**
     * Construct using size bytes of memory for count contexts.
     * @param size bytes of memory to use
     * @param count number of contexts
     * @param scale
     * @param uw
     */
    ContextMap2(uint64_t size, uint32_t count, int scale, uint32_t uw);

    /**
     * Set next whole byte context to ctx.
     * @param ctx
     */
    void set(uint64_t ctx);
    void skip();
    void update() override;
    void setScale(int Scale);
    void mix(Mixer &m);
};

#endif //PAQ8PX_CONTEXTMAP2_HPP
