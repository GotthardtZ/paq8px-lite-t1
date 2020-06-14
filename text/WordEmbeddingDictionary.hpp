#ifndef PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP
#define PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP

#include "../file/FileDisk.hpp"
#include "../file/OpenFromMyFolder.hpp"
#include "Entry.hpp"
#include "Word.hpp"

#ifdef VERBOSE
#include "../Shared.hpp"
#endif

class WordEmbeddingDictionary {
private:
    static constexpr int hashSize = 81929;
    Array<Entry> entries;
    Array<short> table;
    int index;
#ifdef VERBOSE
    uint32_t requests{};
    uint32_t hits{};
    const Shared * const shared;
#endif
    auto findEntry(short prefix, uint8_t suffix) -> int;
    void addEntry(short prefix, uint8_t suffix, int offset);
public:
    WordEmbeddingDictionary();
    ~WordEmbeddingDictionary();
    void reset();
    auto addWord(const Word *w, uint32_t embedding) -> bool;
    void getWordEmbedding(Word *w);
    void loadFromFile(const char *filename);
};

#endif //PAQ8PX_WORDEMBEDDINGDICTIONARY_HPP
