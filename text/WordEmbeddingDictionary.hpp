#ifndef PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP
#define PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP

#include "../file/FileDisk.hpp"
#include "../file/OpenFromMyFolder.hpp"
#include "Entry.hpp"
#include "Word.hpp"

class WordEmbeddingDictionary {
private:
    static constexpr int hashSize = 81929;
    Array<Entry> entries;
    Array<short> table;
    int index;
#ifndef NVERBOSE
    uint32_t requests, hits;
#endif
    int findEntry(short prefix, uint8_t suffix);
    void addEntry(short prefix, uint8_t suffix, int offset = -1);
public:
    WordEmbeddingDictionary();
    ~WordEmbeddingDictionary();
    void reset();
    bool addWord(const Word *w, uint32_t embedding);
    void getWordEmbedding(Word *w);
    void loadFromFile(const char *filename);
};

#endif //PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP
