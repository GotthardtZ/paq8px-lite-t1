#ifndef PAQ8PX_CONTEXTMAP2_HPP
#define PAQ8PX_CONTEXTMAP2_HPP

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

#include "Bucket16.hpp"
#include "HashElementForContextMap.hpp"
#include "Hash.hpp"
#include "Mixer.hpp"
#include "StateMap.hpp"
#include "StateMap1.hpp"
#include "RunMap1.hpp"
#include "StateTable.hpp"
#include "Stretch.hpp"

class ContextMap2 {
public:
    static constexpr int MIXERINPUTS = 3;
    static constexpr uint32_t C = 8;

private:

  static constexpr uint8_t FLAG_DEFERRED_UPDATE = 1;

  struct ContextInfo {
    HashElementForContextMap* slot0; /**< pointer to current byte history in slot0 */
    HashElementForContextMap* slot012; /**< pointer to current bit history states in current slot (either slot0 or slot1 or slot2) */
    uint32_t tableIndex; /**< @ref C whole byte context hashes */
    uint16_t tableChecksum; /**< @ref C whole byte context checksums */
    uint8_t flags;
    HashElementForContextMap bitStateTmp;
  };

    const Shared * const shared;
    Array<Bucket16> hashTable; /**< bit and byte histories (statistics) */
    ContextInfo contextInfoList[C]{};
    const uint32_t mask;
    const int hashBits;

    void updatePendingContextsInSlot(HashElementForContextMap* const p, uint32_t c);
    void updatePendingContexts(uint32_t ctx, uint16_t checksum, uint32_t c);
    size_t getStateByteLocation(const uint32_t bpos, const uint32_t c0);

public:
  int order = 0; // is set after mix()
  uint32_t confidence = 0; // is set after mix()

    /**
     * Construct using @ref size bytes of memory for @ref contexts contexts.
     * @param size bytes of memory to use
     * @param contexts max number of contexts
     * @param scale
     * @param uw
     */
    ContextMap2(const Shared* const sh, uint64_t size);

    /**
     * Set next whole byte context to @ref ctx.
     * @param ctx
     */
    void set(int index, const uint64_t ctx);
    void update();
    void mix(Mixer &m);
    void print();

    RunMap1 runMap1;
    StateMap1 stateMap1;
};

#endif //PAQ8PX_CONTEXTMAP2_HPP
