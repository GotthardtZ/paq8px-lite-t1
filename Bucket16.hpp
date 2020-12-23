#ifndef PAQ8PX_BUCKET16_HPP
#define PAQ8PX_BUCKET16_HPP

#include <cstdint>
#include <cstring>

/**
 * Hash bucket for arbitrary-sized values:
 * For LargeStationaryMap: two 16-bit counts (4 bytes)
 * For LargeIndirectContext: 4 bytes
 *
 */

constexpr int ElementsInBucket = 7;

template<typename T>
struct HashElement { // 6 bytes (when T=uint32_t)
  uint16_t checksum;
  T value;
};

template<typename T>
class Bucket16 { // sizeof(Bucket16) = 7*6 = 42 bytes (when T=uint32_t)
private:
  HashElement<T> elements[ElementsInBucket];
public:

  void reset() {
    memset(&elements[0], 0, sizeof(elements));
  }

  uint32_t* find(uint16_t checksum) {
    checksum += checksum == 0; //don't allow 0 checksums
    for (int i = 0; i < ElementsInBucket; ++i) {
      if (elements[i].checksum == checksum) { // found matching checksum
        if (i != 0) {
          uint32_t value = elements[i].value;
          //shift elements
          memmove(&elements[1], &elements[0], i * sizeof(HashElement<T>));
          //move element to front
          elements[0].checksum = checksum;
          elements[0].value = value;
        }
        return &elements[0].value;
      }
      if (elements[i].checksum == 0) { // found empty slot
        if (i != 0) {
          //shift elements down (make room for the new element)
          memmove(&elements[1], &elements[0], i * sizeof(HashElement<T>));
        }
        goto not_found;
      }
    }
    //no match and no empty slot -> overwrite the last accessed element with an empty one
    memmove(&elements[1], &elements[0], (ElementsInBucket - 1) * sizeof(HashElement<T>));

  not_found:
    //create new element
    elements[0].checksum = checksum;
    elements[0].value = 0;
    return &elements[0].value;
  }
};

#endif //PAQ8PX_BUCKET16_HPP
