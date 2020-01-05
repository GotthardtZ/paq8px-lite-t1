#ifndef PAQ8PX_SEGMENT_HPP
#define PAQ8PX_SEGMENT_HPP

#include <cstdint>
#include "Word.hpp"

class Segment {
public:
    Word FirstWord; // useful following questions
    uint32_t WordCount;
    uint32_t NumCount;
};

#endif //PAQ8PX_SEGMENT_HPP
