#ifndef PAQ8PX_SENTENCE_HPP
#define PAQ8PX_SENTENCE_HPP

#include "Segment.hpp"

class Sentence : public Segment {
public:
    enum Types { // possible sentence types, excluding Imperative
        Declarative, Interrogative, Exclamative, Count
    };
    Types Type;
    uint32_t SegmentCount;
    uint32_t VerbIndex; // relative position of last detected verb
    uint32_t NounIndex; // relative position of last detected noun
    uint32_t CapitalIndex; // relative position of last capitalized word, excluding the initial word of this sentence
    Word lastVerb, lastNoun, lastCapital;
};

#endif //PAQ8PX_SENTENCE_HPP
