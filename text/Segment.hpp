#ifndef PAQ8PX_SEGMENT_HPP
#define PAQ8PX_SEGMENT_HPP

#include <cstdint>
#include "Word.hpp"

class Segment {
public:
    Word firstWord; // useful following questions
    uint32_t wordCount {};
    uint32_t numCount {};
};

#endif //PAQ8PX_SEGMENT_HPP
