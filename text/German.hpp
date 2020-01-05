#ifndef PAQ8PX_GERMAN_HPP
#define PAQ8PX_GERMAN_HPP

#include "Language.hpp"
#include "Word.hpp"

class German : public Language {
private:
    static constexpr int NUM_ABBREV = 3;
    const char *Abbreviations[NUM_ABBREV] = {"fr", "hr", "hrn"};

public:
    enum Flags {
        Adjective = (1U << 2U), Plural = (1U << 3U), Female = (1U << 4U)
    };

    bool isAbbreviation(Word *w) { return w->matchesAny(Abbreviations, NUM_ABBREV); };
};

#endif //PAQ8PX_GERMAN_HPP
