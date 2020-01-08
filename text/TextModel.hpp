#ifndef PAQ8PX_TEXTMODEL_HPP
#define PAQ8PX_TEXTMODEL_HPP

#include <cstdint>
#include <cassert>
#include "Cache.hpp"
#include "../Shared.hpp"
#include "../RingBuffer.hpp"

static constexpr uint8_t asciiGroupC0[2][254] = {{0, 10, 0, 1, 10, 10, 0, 4, 2, 3, 10, 10, 10, 10, 0, 0, 5, 4, 2, 2, 3, 3, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 5, 5, 9, 4, 2, 2, 2, 2, 3, 3, 3, 3, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 5, 8, 8, 5, 9, 9, 6, 5, 2, 2, 2, 2, 2, 2, 2, 8, 3, 3, 3, 3, 3, 3, 3, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 8, 8, 8, 8, 8, 5, 5, 9, 9, 9, 9, 9, 7, 8, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 8, 8, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 8, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                                                 {0, 6,  0, 1, 6,  6,  4, 5, 1, 1, 6,  6,  6,  6,  4, 0, 3, 2, 1, 1, 1, 1, 6,  6,  6,  6,  6,  6,  6,  6,  0, 4, 0, 0, 3, 3, 2, 5, 1, 1, 1, 1, 1, 1, 1, 1, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0, 0, 4, 4, 0, 0, 0, 0, 3, 3, 3, 3, 2, 2, 5, 5, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6}};
static constexpr uint8_t asciiGroup[128] = {0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                            6, 7, 8, 17, 17, 9, 17, 10, 11, 12, 17, 17, 13, 14, 15, 16, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18,
                                            19, 20, 23, 21, 22, 23, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                            2, 2, 24, 27, 25, 27, 26, 27, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
                                            3, 3, 3, 3, 28, 30, 29, 30, 30};

#ifdef USE_TEXTMODEL

class TextModel {
private:
    static constexpr int nCM2 = 28;

public:
    static constexpr int MIXERINPUTS =
            nCM2 * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS + ContextMap2::MIXERINPUTS_BYTE_HISTORY); // 196
    static constexpr int MIXERCONTEXTS = 2048 + 2048 + 4096 + 4096 + 2048 + 2048 + 4096 + 8192 + 2048; //30720
    static constexpr int MIXERCONTEXTSETS = 9;

private:
    const Shared *const shared;
    ModelStats *stats;
    static constexpr uint32_t MIN_RECOGNIZED_WORDS = 4;
    ContextMap2 cm;
    Array<Stemmer *> stemmers;
    Array<Language *> languages;
    Array<WordEmbeddingDictionary *> dictionaries;
    Cache<Word, 8> words[Language::Count];
    Cache<Segment, 4> segments;
    Cache<Sentence, 4> sentences;
    Cache<Paragraph, 2> paragraphs;
    Array<uint32_t> wordPos;
    uint32_t bytePos[256];
    struct WordDistance {
        uint32_t distance[4];
        uint32_t closest;
    };
    Cache<WordDistance, 4> wordDistances;
    Word *cWord, *pWord; // current word, previous word
    Segment *cSegment; // current segment
    Sentence *cSentence; // current sentence
    Paragraph *cParagraph; // current paragraph
    enum Parse {
        Unknown, ReadingWord, PossibleHyphenation, WasAbbreviation, AfterComma, AfterQuote, AfterAbbreviation, ExpectDigit
    } State, pState;
    struct {
        uint32_t count[Language::Count - 1]; // number of recognized words of each language in the last 64 words seen
        uint64_t mask[Language::Count - 1]; // binary mask with the recognition status of the last 64 words for each language
        int id; // current detected language
        int pId; // detected language of the previous word
    } Lang;
    struct {
        uint64_t numbers[2]; // last 2 numbers seen
        uint64_t numHashes[2]; // hashes of the last 2 numbers seen
        uint8_t numLength[2]; // digit length of last 2 numbers seen
        uint32_t numMask; // binary mask of the results of the arithmetic comparisons between the numbers seen
        uint32_t numDiff; // log2 of the consecutive differences between the last 16 numbers seen, clipped to 2 bits per difference
        uint32_t lastUpper; // distance to last uppercase letter
        uint32_t maskUpper; // binary mask of uppercase letters seen (in the last 32 bytes)
        uint32_t lastLetter; // distance to last letter
        uint32_t lastDigit; // distance to last digit
        uint32_t lastPunctuation; // distance to last punctuation character
        uint32_t lastNewLine; // distance to last new line character
        uint32_t prevNewLine; // distance to penultimate new line character
        uint32_t wordGap; // distance between the last words
        uint32_t spaces; // binary mask of whitespace characters seen (in the last 32 bytes)
        uint32_t spaceCount; // count of whitespace characters seen (in the last 32 bytes)
        uint32_t commas; // number of commas seen in this line (not this segment/sentence)
        uint32_t quoteLength; // length (in words) of current quote
        uint32_t maskPunctuation; // mask of relative position of last comma related to other punctuation
        uint32_t nestHash; // hash representing current nesting state
        uint32_t lastNest; // distance to last nesting character
        uint32_t masks[5], wordLength[2];
        int UTF8Remaining; // remaining bytes for current UTF8-encoded Unicode code point (-1 if invalid byte found)
        uint8_t firstLetter; // first letter of current word
        uint8_t firstChar; // first character of current line
        uint8_t expectedDigit; // next expected digit of detected numerical sequence
        uint8_t prevPunctuation; // most recent punctuation character seen
        Word topicDescriptor; // last word before ':'
    } Info;
    uint64_t parseCtx; // state of parser + relevant features used as a context (hash)
    void setContexts();

public:
    TextModel(const Shared *const sh, ModelStats *st, const uint64_t size) : shared(sh), stats(st),
            cm(sh, size, nCM2, 64, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY), stemmers(Language::Count - 1), languages(Language::Count - 1),
            dictionaries(Language::Count - 1), wordPos(0x10000), State(Parse::Unknown), pState(State),
            Lang {{0}, {0}, Language::Unknown, Language::Unknown}, Info {}, parseCtx(0) {
      stemmers[Language::English - 1] = new EnglishStemmer();
      stemmers[Language::French - 1] = new FrenchStemmer();
      stemmers[Language::German - 1] = new GermanStemmer();
      languages[Language::English - 1] = new English();
      languages[Language::French - 1] = new French();
      languages[Language::German - 1] = new German();
      cWord = &words[Lang.id](0);
      pWord = &words[Lang.id](1);
      cSegment = &segments(0);
      cSentence = &sentences(0);
      cParagraph = &paragraphs(0);
      memset(&bytePos[0], 0, sizeof(bytePos));
    }

    ~TextModel() {
      for( int i = 0; i < Language::Count - 1; i++ ) {
        delete stemmers[i];
        delete languages[i];
        delete dictionaries[i];
      }
    }

    void update();

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        update();
        setContexts();
      }
      cm.mix(m);

      const uint8_t characterGroup = stats->Text.characterGroup;
      INJECT_SHARED_c1
      INJECT_SHARED_c0
      m.set(finalize64(hash((Lang.id != Language::Unknown) ? 1 + stemmers[Lang.id - 1]->isVowel(c1) : 0, Info.masks[1] & 0xFF, c0), 11),
            2048);
      m.set(finalize64(hash(ilog2(Info.wordLength[0] + 1), c0, (Info.lastDigit < Info.wordLength[0] + Info.wordGap) |
                                                               ((Info.lastUpper < Info.lastLetter + Info.wordLength[1]) << 1) |
                                                               ((Info.lastPunctuation < Info.wordLength[0] + Info.wordGap) << 2) |
                                                               ((Info.lastUpper < Info.wordLength[0]) << 3)), 11), 2048);
      m.set(finalize64(hash(Info.masks[1] & 0x3FF, characterGroup, Info.lastUpper < Info.wordLength[0],
                            Info.lastUpper < Info.lastLetter + Info.wordLength[1]), 12), 4096);
      m.set(finalize64(hash(Info.spaces & 0x1FF, characterGroup,
                            (Info.lastUpper < Info.wordLength[0]) | ((Info.lastUpper < Info.lastLetter + Info.wordLength[1]) << 1) |
                            ((Info.lastPunctuation < Info.lastLetter) << 2) | ((Info.lastPunctuation < Info.wordLength[0] + Info.wordGap) << 3) |
                            ((Info.lastPunctuation < Info.lastLetter + Info.wordLength[1] + Info.wordGap) << 4)), 12), 4096);
      m.set(finalize64(hash(Info.firstLetter * (Info.wordLength[0] < 4), min(6, Info.wordLength[0]), c0), 11), 2048);
      m.set(finalize64(hash((*pWord)[0], (*pWord)(0), min(4, Info.wordLength[0]), Info.lastPunctuation < Info.lastLetter), 11), 2048);
      m.set(finalize64(hash(min(4, Info.wordLength[0]), characterGroup, Info.lastUpper < Info.wordLength[0],
                            (Info.nestHash > 0) ? Info.nestHash & 0xFF : 0x100 | (Info.firstLetter *
                                                                                  (Info.wordLength[0] > 0 && Info.wordLength[0] < 4))), 12),
            4096);
      m.set(finalize64(hash(characterGroup, Info.masks[4] & 0x1F, (Info.masks[4] >> 5) & 0x1F), 13), 8192);
      m.set(finalize64(hash(characterGroup, uint8_t(pWord->embedding), Lang.id, State), 11), 2048);
    }
};

void TextModel::update() {
  Info.lastUpper = min(0xFF, Info.lastUpper + 1), Info.maskUpper <<= 1;
  Info.lastLetter = min(0x1F, Info.lastLetter + 1);
  Info.lastDigit = min(0xFF, Info.lastDigit + 1);
  Info.lastPunctuation = min(0x3F, Info.lastPunctuation + 1);
  Info.lastNewLine++;
  Info.prevNewLine++;
  Info.lastNest++;
  Info.spaceCount -= (Info.spaces >> 31);
  Info.spaces <<= 1;
  Info.masks[0] <<= 2;
  Info.masks[1] <<= 2;
  Info.masks[2] <<= 4;
  Info.masks[3] <<= 3;
  pState = State;

  INJECT_SHARED_buf
  uint8_t c = buf(1), lc = tolower(c), g = (c < 0x80) ? asciiGroup[c] : 31;
  if( g > 4 || g != (Info.masks[4] & 0x1F))
    Info.masks[4] <<= 5, Info.masks[4] |= g;
  INJECT_SHARED_pos
  bytePos[c] = pos;
  if( c != lc ) {
    c = lc;
    Info.lastUpper = 0, Info.maskUpper |= 1;
  }
  uint8_t pC = buf(2);
  State = Parse::Unknown;
  parseCtx = hash(State, pWord->Hash[0], c, (ilog2(Info.lastNewLine) + 1) * (Info.lastNewLine * 3 > Info.prevNewLine),
                  Info.masks[1] & 0xFC);

  if((c >= 'a' && c <= 'z') || c == APOSTROPHE || c == '-' || c > 0x7F ) {
    if( Info.wordLength[0] == 0 ) {
      // check for hyphenation with "+" (book1 from Calgary)
      if( pC == NEW_LINE &&
          ((Info.lastLetter == 3 && buf(3) == '+') || (Info.lastLetter == 4 && buf(3) == CARRIAGE_RETURN && buf(4) == '+'))) {
        Info.wordLength[0] = Info.wordLength[1];
        for( int i = Language::Unknown; i < Language::Count; i++ )
          words[i]--;
        cWord = pWord, pWord = &words[Lang.pId](1);
        cWord->reset();
        for( uint32_t i = 0; i < Info.wordLength[0]; i++ )
          (*cWord) += buf(Info.wordLength[0] - i + Info.lastLetter);
        Info.wordLength[1] = (*pWord).length();
        cSegment->wordCount--;
        cSentence->wordCount--;
      } else {
        Info.wordGap = Info.lastLetter;
        Info.firstLetter = c;
      }
    }
    Info.lastLetter = 0;
    Info.wordLength[0]++;
    Info.masks[0] += (Lang.id != Language::Unknown) ? 1 + stemmers[Lang.id - 1]->isVowel(c) : 1, Info.masks[1]++, Info.masks[3] +=
            Info.masks[0] & 3;
    if( c == APOSTROPHE ) {
      Info.masks[2] += 12;
      if( Info.wordLength[0] == 1 ) {
        if( Info.quoteLength == 0 && pC == SPACE )
          Info.quoteLength = 1;
        else if( Info.quoteLength > 0 && Info.lastPunctuation == 1 ) {
          Info.quoteLength = 0;
          State = Parse::AfterQuote;
          parseCtx = hash(State, pC);
        }
      }
    }
    (*cWord) += c;
    cWord->calculateWordHash();
    State = Parse::ReadingWord;
    parseCtx = hash(State, cWord->Hash[0]);
  } else {
    if( cWord->length() > 0 ) {
      if( Lang.id != Language::Unknown )
        memcpy(&words[Language::Unknown](0), cWord, sizeof(Word));

      for( int i = Language::Count - 1; i > Language::Unknown; i-- ) {
        Lang.count[i - 1] -= (Lang.mask[i - 1] >> 63), Lang.mask[i - 1] <<= 1;
        if( i != Lang.id )
          memcpy(&words[i](0), cWord, sizeof(Word));
        if( stemmers[i - 1]->stem(&words[i](0)))
          Lang.count[i - 1]++, Lang.mask[i - 1] |= 1;
      }
      Lang.id = Language::Unknown;
      uint32_t best = MIN_RECOGNIZED_WORDS;
      for( int i = Language::Count - 1; i > Language::Unknown; i-- ) {
        if( Lang.count[i - 1] >= best ) {
          best = Lang.count[i - 1] + (i == Lang.pId); //bias to prefer the previously detected language
          Lang.id = i;
        }
        words[i]++;
      }
      words[Language::Unknown]++;

      if( Lang.id != Lang.pId ) {
#ifndef NVERBOSE
        INJECT_STATS_blpos
if (toScreen) printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
        switch (Lang.id) {
        case Language::Unknown: {
          printf("[language: Unknown, blpos: %d]\n", blpos);
          break;
        }
        case Language::English: {
          printf("[language: English, blpos: %d]\n", blpos);
          break;
        }
        case Language::French: {
          printf("[language: French,  blpos: %d]\n", blpos);
          break;
        }
        case Language::German: {
          printf("[language: German,  blpos: %d]\n", blpos);
          break;
        }
        }
#endif
        if( options & OPTION_TRAINTXT && Lang.id != Language::Unknown && dictionaries[Lang.id - 1] == nullptr ) {
          switch( Lang.id ) {
            case Language::English: {
              dictionaries[Lang.id - 1] = new WordEmbeddingDictionary();
              dictionaries[Lang.id - 1]->loadFromFile("english.emb");
            }
          }
        }
      }

      Lang.pId = Lang.id;
      pWord = &words[Lang.id](1);
      cWord = &words[Lang.id](0);
      cWord->reset();
      if( options & OPTION_TRAINTXT ) {
        if( Lang.id != Language::Unknown && dictionaries[Lang.id - 1] != nullptr )
          dictionaries[Lang.id - 1]->getWordEmbedding(pWord);
        wordDistances++;
        for( uint32_t i = wordDistances.size - 1; i > 0; i-- )
          memcpy(&wordDistances(i), &wordDistances(i - 1), sizeof(WordDistance));
        uint32_t minDistance = UINT32_MAX;
        for( uint32_t i = 1; i <= 4; i++ ) {
          uint32_t d = pWord->distanceTo(words[Lang.id](i + 1));
          wordDistances(0).distance[i - 1] = d;
          if( d < minDistance ) {
            minDistance = d;
            wordDistances(0).closest = words[Lang.id](i + 1).embedding;
          }
        }
      }
      wordPos[pWord->Hash[0] & (wordPos.size() - 1)] = pos;
      if( cSegment->wordCount == 0 )
        memcpy(&cSegment->firstWord, pWord, sizeof(Word));
      cSegment->wordCount++;
      if( cSentence->wordCount == 0 )
        memcpy(&cSentence->firstWord, pWord, sizeof(Word));
      cSentence->wordCount++;
      Info.wordLength[1] = Info.wordLength[0], Info.wordLength[0] = 0;
      Info.quoteLength += (Info.quoteLength > 0);
      if( Info.quoteLength > 0x1F )
        Info.quoteLength = 0;
      cSentence->verbIndex++, cSentence->nounIndex++, cSentence->capitalIndex++;
      if((pWord->type & Language::Verb) != 0 ) {
        cSentence->verbIndex = 0;
        memcpy(&cSentence->lastVerb, pWord, sizeof(Word));
      }
      if((pWord->type & Language::Noun) != 0 ) {
        cSentence->nounIndex = 0;
        memcpy(&cSentence->lastNoun, pWord, sizeof(Word));
      }
      if( cSentence->wordCount > 1 && Info.lastUpper < Info.wordLength[1] ) {
        cSentence->capitalIndex = 0;
        memcpy(&cSentence->lastCapital, pWord, sizeof(Word));
      }
    }
    bool skip = false;
    switch( c ) {
      case '.': {
        if( Lang.id != Language::Unknown && Info.lastUpper == Info.wordLength[1] && languages[Lang.id - 1]->isAbbreviation(pWord)) {
          State = Parse::WasAbbreviation;
          parseCtx = hash(State, pWord->Hash[0]);
          break;
        }
      }
      case '?':
      case '!': {
        cSentence->type = (c == '.') ? Sentence::Types::Declarative : (c == '?') ? Sentence::Types::Interrogative
                                                                                 : Sentence::Types::Exclamative;
        cSentence->segmentCount++;
        cParagraph->sentenceCount++;
        cParagraph->typeCount[cSentence->type]++;
        cParagraph->typeMask <<= 2, cParagraph->typeMask |= cSentence->type;
        cSentence = &sentences.next();
        Info.masks[3] += 3;
        skip = true;
      }
      case ',':
      case ';':
      case ':': {
        if( c == ',' ) {
          Info.commas++;
          State = Parse::AfterComma;
          parseCtx = hash(State, ilog2(Info.quoteLength + 1), ilog2(Info.lastNewLine),
                          Info.lastUpper < Info.lastLetter + Info.wordLength[1]);
        } else if( c == ':' )
          memcpy(&Info.topicDescriptor, pWord, sizeof(Word));
        if( !skip ) {
          cSentence->segmentCount++;
          Info.masks[3] += 4;
        }
        Info.lastPunctuation = 0, Info.prevPunctuation = c;
        Info.masks[0] += 3, Info.masks[1] += 2, Info.masks[2] += 15;
        cSegment = &segments.next();
        break;
      }
      case NEW_LINE: {
        Info.prevNewLine = Info.lastNewLine, Info.lastNewLine = 0;
        Info.commas = 0;
        if( Info.prevNewLine == 1 || (Info.prevNewLine == 2 && pC == CARRIAGE_RETURN))
          cParagraph = &paragraphs.next();
        else if((Info.lastLetter == 2 && pC == '+') || (Info.lastLetter == 3 && pC == CARRIAGE_RETURN && buf(3) == '+')) {
          parseCtx = hash(Parse::ReadingWord, pWord->Hash[0]);
          State = Parse::PossibleHyphenation;
        }
      }
      case TAB:
      case CARRIAGE_RETURN:
      case SPACE: {
        Info.spaceCount++, Info.spaces |= 1;
        Info.masks[1] += 3, Info.masks[3] += 5;
        if( c == SPACE && pState == Parse::WasAbbreviation ) {
          State = Parse::AfterAbbreviation;
          parseCtx = hash(State, pWord->Hash[0]);
        }
        break;
      }
      case '(':
        Info.masks[2] += 1;
        Info.masks[3] += 6;
        Info.nestHash += 31;
        Info.lastNest = 0;
        break;
      case '[':
        Info.masks[2] += 2;
        Info.nestHash += 11;
        Info.lastNest = 0;
        break;
      case '{':
        Info.masks[2] += 3;
        Info.nestHash += 17;
        Info.lastNest = 0;
        break;
      case '<':
        Info.masks[2] += 4;
        Info.nestHash += 23;
        Info.lastNest = 0;
        break;
      case 0xAB:
        Info.masks[2] += 5;
        break;
      case ')':
        Info.masks[2] += 6;
        Info.nestHash -= 31;
        Info.lastNest = 0;
        break;
      case ']':
        Info.masks[2] += 7;
        Info.nestHash -= 11;
        Info.lastNest = 0;
        break;
      case '}':
        Info.masks[2] += 8;
        Info.nestHash -= 17;
        Info.lastNest = 0;
        break;
      case '>':
        Info.masks[2] += 9;
        Info.nestHash -= 23;
        Info.lastNest = 0;
        break;
      case 0xBB:
        Info.masks[2] += 10;
        break;
      case QUOTE: {
        Info.masks[2] += 11;
        // start/stop counting
        if( Info.quoteLength == 0 )
          Info.quoteLength = 1;
        else {
          Info.quoteLength = 0;
          State = Parse::AfterQuote;
          parseCtx = hash(State, 0x100 | pC);
        }
        break;
      }
      case '/':
      case '-':
      case '+':
      case '*':
      case '=':
      case '%':
        Info.masks[2] += 13;
        break;
      case '\\':
      case '|':
      case '_':
      case '@':
      case '&':
      case '^':
        Info.masks[2] += 14;
        break;
    }
    if( c >= '0' && c <= '9' ) {
      Info.numbers[0] = Info.numbers[0] * 10 + (c & 0xF), Info.numLength[0] = min(19, Info.numLength[0] + 1);
      Info.numHashes[0] = combine64(Info.numHashes[0], c);
      Info.expectedDigit = -1;
      if( Info.numLength[0] < Info.numLength[1] && (pState == Parse::ExpectDigit || ((Info.numDiff & 3) == 0 && Info.numLength[0] <= 1))) {
        uint64_t expectedNum = Info.numbers[1] + (Info.numMask & 3) - 2, placeDivisor = 1;
        for( int i = 0; i < Info.numLength[1] - Info.numLength[0]; i++, placeDivisor *= 10 );
        if( expectedNum / placeDivisor == Info.numbers[0] ) {
          placeDivisor /= 10;
          Info.expectedDigit = (expectedNum / placeDivisor) % 10;
          State = Parse::ExpectDigit;
        }
      } else {
        uint8_t d = buf(Info.numLength[0] + 2);
        if( Info.numLength[0] < 3 && buf(Info.numLength[0] + 1) == ',' && d >= '0' && d <= '9' )
          State = Parse::ExpectDigit;
      }
      Info.lastDigit = 0;
      Info.masks[3] += 7;
    } else if( Info.numbers[0] > 0 ) {
      Info.numMask <<= 2, Info.numMask |= 1 + (Info.numbers[0] >= Info.numbers[1]) + (Info.numbers[0] > Info.numbers[1]);
      Info.numDiff <<= 2, Info.numDiff |= min(3, ilog2(abs((int) (Info.numbers[0] - Info.numbers[1]))));
      Info.numbers[1] = Info.numbers[0], Info.numbers[0] = 0;
      Info.numHashes[1] = Info.numHashes[0], Info.numHashes[0] = 0;
      Info.numLength[1] = Info.numLength[0], Info.numLength[0] = 0;
      cSegment->numCount++, cSentence->numCount++;
    }
  }
  if( Info.lastNewLine == 1 )
    Info.firstChar = (Lang.id != Language::Unknown) ? c : min(c, 96);
  if( Info.lastNest > 512 )
    Info.nestHash = 0;
  int leadingBitsSet = 0;
  while(((c >> (7 - leadingBitsSet)) & 1) != 0 )
    leadingBitsSet++;

  if( Info.UTF8Remaining > 0 && leadingBitsSet == 1 )
    Info.UTF8Remaining--;
  else
    Info.UTF8Remaining = (leadingBitsSet != 1) ? (c != 0xC0 && c != 0xC1 && c < 0xF5) ? (leadingBitsSet - (leadingBitsSet > 0)) : -1 : 0;
  Info.maskPunctuation = (bytePos[','] > bytePos['.']) | ((bytePos[','] > bytePos['!']) << 1) | ((bytePos[','] > bytePos['?']) << 2) |
                         ((bytePos[','] > bytePos[':']) << 3) | ((bytePos[','] > bytePos[';']) << 4);

  stats->Text.firstLetter = Info.firstLetter;
  stats->Text.mask = Info.masks[1] & 0xFF;
}

void TextModel::setContexts() {
  INJECT_SHARED_buf
  const uint8_t c = buf(1), lc = tolower(c), m2 = Info.masks[2] & 0xF, column = min(0xFF, Info.lastNewLine);
  const uint64_t w = State == Parse::ReadingWord ? cWord->Hash[0] : pWord->Hash[0];

  const uint64_t cWordHash0 = cWord->Hash[0];
  const uint64_t pWordHash0 = pWord->Hash[0];
  const uint64_t pWordHash1 = pWord->Hash[1];

  cm.set(parseCtx);
  uint64_t i = State * 64;
  cm.set(hash(++i, cWordHash0, pWordHash0,
              (Info.lastUpper < Info.wordLength[0]) | ((Info.lastDigit < Info.wordLength[0] + Info.wordGap) << 1)));
  cm.set(hash(++i, cWordHash0, words[Lang.pId](2).Hash[0], min(10, ilog2((uint32_t) Info.numbers[0])),
              (Info.lastUpper < Info.lastLetter + Info.wordLength[1]) | ((Info.lastLetter > 3) << 1) |
              ((Info.lastLetter > 0 && Info.wordLength[1] < 3) << 2)));
  cm.set(hash(++i, cWordHash0, Info.masks[1] & 0x3FF, words[Lang.pId](3).Hash[1],
              (Info.lastDigit < Info.wordLength[0] + Info.wordGap) | ((Info.lastUpper < Info.lastLetter + Info.wordLength[1]) << 1) |
              ((Info.spaces & 0x7F) << 2)));
  cm.set(hash(++i, cWordHash0, pWordHash1));
  cm.set(hash(++i, cWordHash0, pWordHash1, words[Lang.pId](2).Hash[1]));
  cm.set(hash(++i, w, words[Lang.pId](2).Hash[0], words[Lang.pId](3).Hash[0]));
  cm.set(hash(++i, cWordHash0, c, (cSentence->verbIndex < cSentence->wordCount) ? cSentence->lastVerb.Hash[0] : 0));
  cm.set(hash(++i, pWordHash1, Info.masks[1] & 0xFC, lc, Info.wordGap));
  cm.set(hash(++i, (Info.lastLetter == 0) ? cWordHash0 : pWordHash0, c, cSegment->firstWord.Hash[1],
              min(3, ilog2(cSegment->wordCount + 1))));
  cm.set(hash(++i, cWordHash0, c, segments(1).firstWord.Hash[1]));
  cm.set(hash(++i, max(31, lc), Info.masks[1] & 0xFFC, (Info.spaces & 0xFE) | (Info.lastPunctuation < Info.lastLetter),
              (Info.maskUpper & 0xFF) | (((0x100 | Info.firstLetter) * (Info.wordLength[0] > 1)) << 8)));
  cm.set(hash(++i, column, min(7, ilog2(Info.lastUpper + 1)), ilog2(Info.lastPunctuation + 1)));
  cm.set(hash(++i,
              (column & 0xF8) | (Info.masks[1] & 3) | ((Info.prevNewLine - Info.lastNewLine > 63) << 2) | (min(3, Info.lastLetter) << 8) |
              (Info.firstChar << 10) | ((Info.commas > 4) << 18) | ((m2 >= 1 && m2 <= 5) << 19) | ((m2 >= 6 && m2 <= 10) << 20) |
              ((m2 == 11 || m2 == 12) << 21) | ((Info.lastUpper < column) << 22) | ((Info.lastDigit < column) << 23) |
              ((column < Info.prevNewLine - Info.lastNewLine) << 24)));
  cm.set(hash(++i, (2 * column) / 3, min(13, Info.lastPunctuation) + (Info.lastPunctuation > 16) + (Info.lastPunctuation > 32) + Info.maskPunctuation * 16,
              ilog2(Info.lastUpper + 1), ilog2(Info.prevNewLine - Info.lastNewLine),
              ((Info.masks[1] & 3) == 0) | ((m2 < 6) << 1) | ((m2 < 11) << 2)));
  cm.set(hash(++i, column >> 1, Info.spaces & 0xF));
  cm.set(hash(++i, Info.masks[3] & 0x3F, min((max(Info.wordLength[0], 3) - 2) * (Info.wordLength[0] < 8), 3),
              Info.firstLetter * (Info.wordLength[0] < 5), w,
              (c == buf(2)) | ((Info.masks[2] > 0) << 1) | ((Info.lastPunctuation < Info.wordLength[0] + Info.wordGap) << 2) |
              ((Info.lastUpper < Info.wordLength[0]) << 3) | ((Info.lastDigit < Info.wordLength[0] + Info.wordGap) << 4) |
              ((Info.lastPunctuation < 2 + Info.wordLength[0] + Info.wordGap + Info.wordLength[1]) << 5)));
  cm.set(hash(++i, w, c, Info.numHashes[1]));
  INJECT_SHARED_pos
  cm.set(hash(++i, w, c, llog(pos - wordPos[w & (wordPos.size() - 1)]) >> 1));
  cm.set(hash(++i, w, c, Info.topicDescriptor.Hash[0]));
  cm.set(hash(++i, Info.numLength[0], c, Info.topicDescriptor.Hash[0]));
  cm.set(hash(++i, (Info.lastLetter > 0) ? c : 0x100, Info.masks[1] & 0xFFC, Info.nestHash & 0x7FF));
  cm.set(hash(++i, w, c, Info.masks[3] & 0x1FF,
              ((cSentence->verbIndex == 0 && cSentence->lastVerb.length() > 0) << 6) | ((Info.wordLength[1] > 3) << 5) |
              ((cSegment->wordCount == 0) << 4) | ((cSentence->segmentCount == 0 && cSentence->wordCount < 2) << 3) |
              ((Info.lastPunctuation >= Info.lastLetter + Info.wordLength[1] + Info.wordGap) << 2) |
              ((Info.lastUpper < Info.lastLetter + Info.wordLength[1]) << 1) |
              (Info.lastUpper < Info.wordLength[0] + Info.wordGap + Info.wordLength[1])));
  cm.set(hash(++i, c, pWordHash1, Info.firstLetter * (Info.wordLength[0] < 6), ((Info.lastPunctuation < Info.wordLength[0] + Info.wordGap) << 1) |
                                                                               (Info.lastPunctuation >=
                                                                                Info.lastLetter + Info.wordLength[1] + Info.wordGap)));
  cm.set(hash(++i, w, c, words[Lang.pId](1 + (Info.wordLength[0] == 0)).letters[words[Lang.pId](1 + (Info.wordLength[0] == 0)).start],
              Info.firstLetter * (Info.wordLength[0] < 7)));
  cm.set(hash(++i, column, Info.spaces & 7, Info.nestHash & 0x7FF));
  cm.set(hash(++i, cWordHash0, (Info.lastUpper < column) | ((Info.lastUpper < Info.wordLength[0]) << 1), min(5, Info.wordLength[0])));
  cm.set(hash(++i, Lang.id, w, uint8_t(words[Lang.id](1 + (State != Parse::ReadingWord)).embedding),
              (Info.lastUpper < Info.wordLength[0]) | ((cSegment->wordCount == 0) << 1)));
  assert(i - State * 64 + 1 == nCM2);
}

#else
class TextModel {
public:
    static constexpr int MIXERINPUTS = 0;
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
};
#endif //USE_TEXTMODEL
#endif //PAQ8PX_TEXTMODEL_HPP
