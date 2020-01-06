#ifndef PAQ8PX_LANGUAGE_HPP
#define PAQ8PX_LANGUAGE_HPP

class Language {
public:
    enum Flags {
        Verb = (1U << 0U), Noun = (1U << 1U)
    };
    enum Ids {
        Unknown, English, French, German, Count
    };

    virtual ~Language() = default;
    virtual bool isAbbreviation(Word *w) = 0;
};

#endif //PAQ8PX_LANGUAGE_HPP
