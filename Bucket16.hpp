#ifndef PAQ8PX_BUCKET16_HPP
#define PAQ8PX_BUCKET16_HPP

#include <cstdint>
#include <cstring>
#include "HashElementForContextMap.hpp"

/**
 * Hash bucket to be used in a hash table
 * A hash bucket consists of a list of HashElements
 * Each hash element consists of a 16-bit checksum for collision detection
 * and for ContextMap2: bit and byte statistics ( sizeof(HashElement) = 2+7 = 9 bytes )
 *
 */

#pragma pack(push,1)
template<typename T>
struct HashElement {
  uint16_t checksum;
  T value;
};
#pragma pack(pop)

class Bucket16 {
private:
  static constexpr int ElementsInBucket = 7;
  HashElement<HashElementForContextMap> elements[ElementsInBucket];
  uint8_t unused{};
public:

  void reset() {
    for (size_t i = 0; i < ElementsInBucket; i++)
      elements[i] = {};
  }

  void stat(uint64_t& used, uint64_t& empty) {
    for (size_t i = 0; i < ElementsInBucket; i++)
      if (elements[i].checksum == 0)
        empty++;
      else
        used++;
  }

  HashElementForContextMap* find(uint16_t checksum) {

    checksum += checksum == 0; //don't allow 0 checksums (0 checksums are used for empty slots)

    if (elements[0].checksum == checksum) //there is a high chance that we'll find it in the first slot, so go for it
      return &elements[0].value;

    uint8_t minPrio = 255;
    size_t minElementIdx = 1;
    for (size_t i = 1; i < ElementsInBucket; ++i) {
      if (elements[i].checksum == checksum) { // found matching checksum
        HashElementForContextMap value = elements[i].value;
        //shift elements down
        memmove(&elements[1], &elements[0], i * sizeof(HashElement<HashElementForContextMap>));
        //move element to front (re-create)
        elements[0].checksum = checksum;
        elements[0].value = value;
        return &elements[0].value;
      }
      if (elements[i].checksum == 0) { // found empty slot
        //shift elements down (free the first slot for the new element)
        memmove(&elements[1], &elements[0], i * sizeof(HashElement<HashElementForContextMap>)); // i==0 is OK
        goto create_element;
      }
      uint8_t thisPrio = elements[i].value.prio();
      if (thisPrio < minPrio) { // "<" a little bit faster, sometimes even better
        minPrio = thisPrio;
        minElementIdx = i;
      }
    }

    memmove(&elements[1], &elements[0], minElementIdx * sizeof(HashElement<HashElementForContextMap>));

  create_element:
    elements[0].checksum = checksum;
    elements[0].value = {};
    return &elements[0].value;
  }
};

#endif //PAQ8PX_BUCKET16_HPP
