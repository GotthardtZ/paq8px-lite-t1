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
    // TODO: Split this into its own class
    class Bucket { // hash bucket, 64 bytes
        uint16_t checksums[7]; // byte context checksums
        uint8_t mostRecentlyUsed; // last 2 accesses (0-6) in low, high nibble
    public:
        uint8_t bitState[7][7]; // byte context, 3-bit context -> bit history state
        // bitState[][0] = 1st bit, bitState[][1,2] = 2nd bit, bitState[][3..6] = 3rd bit
        // bitState[][0] is also a replacement priority, 0 = empty
        inline uint8_t *find(uint16_t checksum) { // find or create hash element matching checksum.
          // If not found, insert or replace lowest priority (skipping 2 most recent).
          if( checksums[mostRecentlyUsed & 15U] == checksum )
            return &bitState[mostRecentlyUsed & 15U][0];
          int worst = 0xFFFF, idx = 0;
          for( int i = 0; i < 7; ++i ) {
            if( checksums[i] == checksum ) {
              mostRecentlyUsed = mostRecentlyUsed << 4U | i;
              return &bitState[i][0];
            }
            if( bitState[i][0] < worst && (mostRecentlyUsed & 15U) != i && mostRecentlyUsed >> 4U != i ) {
              worst = bitState[i][0];
              idx = i;
            }
          }
          mostRecentlyUsed = 0xF0U | idx;
          checksums[idx] = checksum;
          return (uint8_t *) memset(&bitState[idx][0], 0, 7);
        }
    };

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
