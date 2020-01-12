#ifndef PAQ8PX_PARAGRAPH_HPP
#define PAQ8PX_PARAGRAPH_HPP

#include "Sentence.hpp"
#include <cstdint>

class Paragraph {
public:
    uint32_t sentenceCount, typeCount[Sentence::Types::Count], typeMask;
};

#endif //PAQ8PX_PARAGRAPH_HPP
