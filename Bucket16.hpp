#ifndef PAQ8PX_BUCKET16_HPP
#define PAQ8PX_BUCKET16_HPP

#include <cstdint>
#include <cstring>

/**
 * Hash bucket for 16-bit counts
 *
 */

constexpr int ElementsInBucket = 7;

struct HashElement16 { // 6 bytes
  uint16_t checksum;
  uint32_t value;
};

class Bucket16 { // sizeof(Bucket16) = 7*6 = 42 bytes
private:
  HashElement16 elements[ElementsInBucket];
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
          memmove(&elements[1], &elements[0], i * sizeof(HashElement16));
          //move element to front
          elements[0].checksum = checksum;
          elements[0].value = value;
        }
        return &elements[0].value;
      }
      if (elements[i].checksum == 0) { // found empty slot
        if (i != 0) {
          //shift elements down (make room for the new element)
          memmove(&elements[1], &elements[0], i * sizeof(HashElement16));
        }
        //create new element
        elements[0].checksum = checksum;
        elements[0].value = 0;
        return &elements[0].value;
      }
    }
    //no match and no empty slot -> overwrite the last accessed element with an empty one
    memmove(&elements[1], &elements[0], (ElementsInBucket - 1) * sizeof(HashElement16));
    //create new element
    elements[0].checksum = checksum;
    elements[0].value = 0;
    return &elements[0].value;
  }
};

#endif //PAQ8PX_BUCKET16_HPP
