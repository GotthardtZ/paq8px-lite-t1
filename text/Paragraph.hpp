#ifndef PAQ8PX_PARAGRAPH_HPP
#define PAQ8PX_PARAGRAPH_HPP

class Paragraph {
public:
    uint32_t SentenceCount, TypeCount[Sentence::Types::Count], TypeMask;
};

#endif //PAQ8PX_PARAGRAPH_HPP
