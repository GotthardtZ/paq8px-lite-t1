#ifndef PAQ8PX_ENGLISH_HPP
#define PAQ8PX_ENGLISH_HPP

class English : public Language {
private:
    static constexpr int NUM_ABBREV = 6;
    const char *Abbreviations[NUM_ABBREV] = {"mr", "mrs", "ms", "dr", "st", "jr"};

public:
    enum Flags {
        Adjective = (1U << 2U),
        Plural = (1U << 3U),
        Male = (1U << 4U),
        Female = (1U << 5U),
        Negation = (1U << 6U),
        PastTense = (1U << 7U) | Verb,
        PresentParticiple = (1 << 8) | Verb,
        AdjectiveSuperlative = (1 << 9) | Adjective,
        AdjectiveWithout = (1 << 10) | Adjective,
        AdjectiveFull = (1 << 11) | Adjective,
        AdverbOfManner = (1 << 12),
        SuffixNESS = (1 << 13),
        SuffixITY = (1 << 14) | Noun,
        SuffixCapable = (1 << 15),
        SuffixNCE = (1 << 16),
        SuffixNT = (1 << 17),
        SuffixION = (1 << 18),
        SuffixAL = (1 << 19) | Adjective,
        SuffixIC = (1 << 20) | Adjective,
        SuffixIVE = (1 << 21),
        SuffixOUS = (1 << 22) | Adjective,
        PrefixOver = (1 << 23),
        PrefixUnder = (1 << 24)
    };

    bool isAbbreviation(Word *w) { return w->matchesAny(Abbreviations, NUM_ABBREV); };
};

#endif //PAQ8PX_ENGLISH_HPP
