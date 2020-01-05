#ifndef PAQ8PX_FRENCH_HPP
#define PAQ8PX_FRENCH_HPP

class French : public Language {
private:
    static constexpr int NUM_ABBREV = 2;
    const char *Abbreviations[NUM_ABBREV] = {"m", "mm"};

public:
    enum Flags {
        Adjective = (1U << 2U), Plural = (1U << 3U)
    };

    bool isAbbreviation(Word *w) override { return w->matchesAny(Abbreviations, NUM_ABBREV); };
};

#endif //PAQ8PX_FRENCH_HPP
