#ifndef PAQ8PX_PARAGRAPH_HPP
#define PAQ8PX_PARAGRAPH_HPP

class Paragraph {
public:
    uint32_t sentenceCount, typeCount[Sentence::Types::Count], typeMask;
};

#endif //PAQ8PX_PARAGRAPH_HPP
