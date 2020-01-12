#ifndef PAQ8PX_SEGMENT_HPP
#define PAQ8PX_SEGMENT_HPP

#include "Word.hpp"
#include <cstdint>

class Segment {
public:
    Word firstWord; // useful following questions
    uint32_t wordCount {};
    uint32_t numCount {};
};

#endif //PAQ8PX_SEGMENT_HPP
