#ifndef PAQ8PX_WORD_HPP
#define PAQ8PX_WORD_HPP

#include <cmath>
#include "../Hash.hpp"
#include <cstdio>
#include <cctype>

#define MAX_WORD_SIZE 64
#define WORD_EMBEDDING_SIZE 3

class Word {
private:
    uint64_t calculateHash();
public:
    uint8_t letters[MAX_WORD_SIZE] {};
    uint8_t start {}, end {};
    uint64_t Hash[2] {};
    uint32_t type {}, language {}, embedding {};
    Word();
    void reset();
    bool operator==(const char *s) const;
    bool operator!=(const char *s) const;
    void operator+=(char c);
    uint32_t operator-(Word w) const;
    uint32_t operator+(Word w) const;
    uint8_t operator[](uint8_t i) const;
    uint8_t operator()(uint8_t i) const;
    [[nodiscard]] uint32_t length() const;
    [[nodiscard]] uint32_t distanceTo(Word w) const;
    void calculateWordHash();

    /**
     * Called by a stemmer after stemming
     */
    void calculateStemHash();
    bool changeSuffix(const char *oldSuffix, const char *newSuffix);
    bool matchesAny(const char **a, int count);
    bool endsWith(const char *suffix) const;
    bool startsWith(const char *prefix) const;
    void print() const;
};

#endif //PAQ8PX_WORD_HPP
