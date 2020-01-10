#ifndef PAQ8PX_ENGLISHSTEMMER_HPP
#define PAQ8PX_ENGLISHSTEMMER_HPP

#include <cstdint>
#include <cctype>
#include "English.hpp"
#include "Stemmer.hpp"

/**
 * English affix stemmer, based on the Porter2 stemmer.
 */
class EnglishStemmer : public Stemmer {
private:
    static constexpr int NUM_VOWELS = 6;
    static constexpr char vowels[NUM_VOWELS] = {'a', 'e', 'i', 'o', 'u', 'y'};
    static constexpr int NUM_DOUBLES = 9;
    static constexpr char doubles[NUM_DOUBLES] = {'b', 'd', 'f', 'g', 'm', 'n', 'p', 'r', 't'};
    static constexpr int NUM_LI_ENDINGS = 10;
    static constexpr char liEndings[NUM_LI_ENDINGS] = {'c', 'd', 'e', 'g', 'h', 'k', 'm', 'n', 'r', 't'};
    static constexpr int NUM_NON_SHORT_CONSONANTS = 3;
    static constexpr char nonShortConsonants[NUM_NON_SHORT_CONSONANTS] = {'w', 'x', 'Y'};
    static constexpr int NUM_MALE_WORDS = 9;
    const char *maleWords[NUM_MALE_WORDS] = {"he", "him", "his", "himself", "man", "men", "boy", "husband", "actor"};
    static constexpr int NUM_FEMALE_WORDS = 8;
    const char *femaleWords[NUM_FEMALE_WORDS] = {"she", "her", "herself", "woman", "women", "girl", "wife", "actress"};
    static constexpr int NUM_COMMON_WORDS = 12;
    const char *commonWords[NUM_COMMON_WORDS] = {"the", "be", "to", "of", "and", "in", "that", "you", "have", "with", "from", "but"};
    static constexpr int NUM_SUFFIXES_STEP0 = 3;
    const char *suffixesStep0[NUM_SUFFIXES_STEP0] = {"'s'", "'s", "'"};
    static constexpr int NUM_SUFFIXES_STEP1b = 6;
    const char *suffixesStep1B[NUM_SUFFIXES_STEP1b] = {"eedly", "eed", "ed", "edly", "ing", "ingly"};
    static constexpr uint32_t TypesStep1b[NUM_SUFFIXES_STEP1b] = {English::AdverbOfManner, 0, English::PastTense,
                                                                  English::AdverbOfManner | English::PastTense, English::PresentParticiple,
                                                                  English::AdverbOfManner | English::PresentParticiple};
    static constexpr int NUM_SUFFIXES_STEP2 = 22;
    const char *(suffixesStep2[NUM_SUFFIXES_STEP2])[2] = {{"ization", "ize"},
                                                          {"ational", "ate"},
                                                          {"ousness", "ous"},
                                                          {"iveness", "ive"},
                                                          {"fulness", "ful"},
                                                          {"tional",  "tion"},
                                                          {"lessli",  "less"},
                                                          {"biliti",  "ble"},
                                                          {"entli",   "ent"},
                                                          {"ation",   "ate"},
                                                          {"alism",   "al"},
                                                          {"aliti",   "al"},
                                                          {"fulli",   "ful"},
                                                          {"ousli",   "ous"},
                                                          {"iviti",   "ive"},
                                                          {"enci",    "ence"},
                                                          {"anci",    "ance"},
                                                          {"abli",    "able"},
                                                          {"izer",    "ize"},
                                                          {"ator",    "ate"},
                                                          {"alli",    "al"},
                                                          {"bli",     "ble"}};
    const uint32_t typesStep2[NUM_SUFFIXES_STEP2] = {English::SuffixION, English::SuffixION | English::SuffixAL, English::SuffixNESS,
                                                     English::SuffixNESS, English::SuffixNESS, English::SuffixION | English::SuffixAL,
                                                     English::AdverbOfManner, English::AdverbOfManner | English::SuffixITY,
                                                     English::AdverbOfManner, English::SuffixION, 0, English::SuffixITY,
                                                     English::AdverbOfManner, English::AdverbOfManner, English::SuffixITY, 0, 0,
                                                     English::AdverbOfManner, 0, 0, English::AdverbOfManner, English::AdverbOfManner};
    static constexpr int NUM_SUFFIXES_STEP3 = 8;
    const char *(suffixesStep3[NUM_SUFFIXES_STEP3])[2] = {{"ational", "ate"},
                                                          {"tional",  "tion"},
                                                          {"alize",   "al"},
                                                          {"icate",   "ic"},
                                                          {"iciti",   "ic"},
                                                          {"ical",    "ic"},
                                                          {"ful",     ""},
                                                          {"ness",    ""}};
    static constexpr uint32_t typesStep3[NUM_SUFFIXES_STEP3] = {English::SuffixION | English::SuffixAL,
                                                                English::SuffixION | English::SuffixAL, 0, 0, English::SuffixITY,
                                                                English::SuffixAL, English::AdjectiveFull, English::SuffixNESS};
    static constexpr int NUM_SUFFIXES_STEP4 = 20;
    const char *suffixesStep4[NUM_SUFFIXES_STEP4] = {"al", "ance", "ence", "er", "ic", "able", "ible", "ant", "ement", "ment", "ent", "ou",
                                                     "ism", "ate", "iti", "ous", "ive", "ize", "sion", "tion"};
    static constexpr uint32_t typesStep4[NUM_SUFFIXES_STEP4] = {English::SuffixAL, English::SuffixNCE, English::SuffixNCE, 0,
                                                                English::SuffixIC, English::SuffixCapable, English::SuffixCapable,
                                                                English::SuffixNT, 0, 0, English::SuffixNT, 0, 0, 0, English::SuffixITY,
                                                                English::SuffixOUS, English::SuffixIVE, 0, English::SuffixION,
                                                                English::SuffixION};
    static constexpr int NUM_EXCEPTION_REGION1 = 3;
    const char *exceptionsRegion1[NUM_EXCEPTION_REGION1] = {"gener", "arsen", "commun"};
    static constexpr int NUM_EXCEPTIONS1 = 19;
    const char *(exceptions1[NUM_EXCEPTIONS1])[2] = {{"skis",   "ski"},
                                                     {"skies",  "sky"},
                                                     {"dying",  "die"},
                                                     {"lying",  "lie"},
                                                     {"tying",  "tie"},
                                                     {"idly",   "idl"},
                                                     {"gently", "gentl"},
                                                     {"ugly",   "ugli"},
                                                     {"early",  "earli"},
                                                     {"only",   "onli"},
                                                     {"singly", "singl"},
                                                     {"sky",    "sky"},
                                                     {"news",   "news"},
                                                     {"howe",   "howe"},
                                                     {"atlas",  "atlas"},
                                                     {"cosmos", "cosmos"},
                                                     {"bias",   "bias"},
                                                     {"andes",  "andes"},
                                                     {"texas",  "texas"}};
    static constexpr uint32_t typesExceptions1[NUM_EXCEPTIONS1] = {English::Noun | English::Plural,
                                                                   English::Noun | English::Plural | English::Verb,
                                                                   English::PresentParticiple, English::PresentParticiple,
                                                                   English::PresentParticiple, English::AdverbOfManner,
                                                                   English::AdverbOfManner, English::Adjective,
                                                                   English::Adjective | English::AdverbOfManner, 0, English::AdverbOfManner,
                                                                   English::Noun, English::Noun, 0, English::Noun, English::Noun,
                                                                   English::Noun, English::Noun | English::Plural, English::Noun};
    static constexpr int NUM_EXCEPTIONS2 = 8;
    const char *exceptions2[NUM_EXCEPTIONS2] = {"inning", "outing", "canning", "herring", "earring", "proceed", "exceed", "succeed"};
    static constexpr uint32_t typesExceptions2[NUM_EXCEPTIONS2] = {English::Noun, English::Noun, English::Noun, English::Noun,
                                                                   English::Noun, English::Verb, English::Verb, English::Verb};

    inline bool isConsonant(char c);
    static inline bool isShortConsonant(char c);
    static inline bool isDouble(char c);
    static inline bool isLiEnding(char c);
    uint32_t getRegion1(const Word *w);
    bool endsInShortSyllable(const Word *w);
    bool isShortWord(const Word *w);
    inline bool hasVowels(const Word *w);
    static bool trimApostrophes(Word *w);
    void markYsAsConsonants(Word *w);
    static bool processPrefixes(Word *w);
    bool processSuperlatives(Word *w);

    /**
     * Search for the longest among the suffixes, 's' or 's or ' and remove if found.
     * Examples: Children's toys / Vice presidents' duties
     * @param w
     * @return
     */
    bool step0(Word *w);

    /**
     * Search for the longest among the following suffixes, and perform the action indicated.
     * @param w
     * @return
     */
    bool step1A(Word *w);

    /**
     * Search for the longest among the following suffixes, and perform the action indicated.
     * @param w
     * @param r1
     * @return
     */
    bool step1B(Word *w, uint32_t r1);

    bool step1C(Word *w);

    bool step2(Word *w, uint32_t r1);

    bool step3(Word *w, uint32_t r1, uint32_t r2);

    /**
     * Search for the longest among the following suffixes, and, if found and in r2, perform the action indicated.
     * @param w
     * @param r2
     * @return
     */
    bool step4(Word *w, uint32_t r2);

    /**
     * Search for the the following suffixes, and, if found, perform the action indicated.
     * @param w
     * @param r1
     * @param r2
     * @return
     */
    bool step5(Word *w, uint32_t r1, uint32_t r2);

public:
    inline bool isVowel(char c) final;
    bool stem(Word *w) override;
};

#endif //PAQ8PX_ENGLISHSTEMMER_HPP
