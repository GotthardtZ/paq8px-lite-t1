#ifndef PAQ8PX_GERMANSTEMMER_HPP
#define PAQ8PX_GERMANSTEMMER_HPP

#include <cstdint>
#include <cctype>
#include "Language.hpp"
#include "German.hpp"
#include "Word.hpp"
#include "Stemmer.hpp"

/**
 * German suffix stemmer, based on the Porter stemmer.
 */
class GermanStemmer : public Stemmer {
private:
    static constexpr int NUM_VOWELS = 9;
    static constexpr char vowels[NUM_VOWELS] = {'a', 'e', 'i', 'o', 'u', 'y', '\xE4', '\xF6', '\xFC'};
    static constexpr int NUM_COMMON_WORDS = 10;
    const char *commonWords[NUM_COMMON_WORDS] = {"der", "die", "das", "und", "sie", "ich", "mit", "sich", "auf", "nicht"};
    static constexpr int NUM_ENDINGS = 10;
    static constexpr char endings[NUM_ENDINGS] = {'b', 'd', 'f', 'g', 'h', 'k', 'l', 'm', 'n', 't'}; //plus 'r' for words ending in 's'
    static constexpr int NUM_SUFFIXES_STEP1 = 6;
    const char *suffixesStep1[NUM_SUFFIXES_STEP1] = {"em", "ern", "er", "e", "en", "es"};
    static constexpr int NUM_SUFFIXES_STEP2 = 3;
    const char *suffixesStep2[NUM_SUFFIXES_STEP2] = {"en", "er", "est"};
    static constexpr int NUM_SUFFIXES_STEP3 = 7;
    const char *suffixesStep3[NUM_SUFFIXES_STEP3] = {"end", "ung", "ik", "ig", "isch", "lich", "heit"};

    void convertUtf8(Word *w);
    static void replaceSharpS(Word *w);
    void markVowelsAsConsonants(Word *w);
    static inline bool isValidEnding(char c, bool includeR = false);
    bool step1(Word *w, uint32_t r1);
    bool step2(Word *w, uint32_t r1);
    bool step3(Word *w, uint32_t r1, uint32_t r2);

public:
    inline bool isVowel(char c) final;
    bool stem(Word *w) override;
};

#endif //PAQ8PX_GERMANSTEMMER_HPP
