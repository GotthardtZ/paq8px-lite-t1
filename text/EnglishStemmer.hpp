#ifndef PAQ8PX_ENGLISHSTEMMER_HPP
#define PAQ8PX_ENGLISHSTEMMER_HPP

//  English affix stemmer, based on the Porter2 stemmer.

#include <cstdint>

class EnglishStemmer : public Stemmer {
private:
    static constexpr int NUM_VOWELS = 6;
    static constexpr char Vowels[NUM_VOWELS] = {'a', 'e', 'i', 'o', 'u', 'y'};
    static constexpr int NUM_DOUBLES = 9;
    static constexpr char Doubles[NUM_DOUBLES] = {'b', 'd', 'f', 'g', 'm', 'n', 'p', 'r', 't'};
    static constexpr int NUM_LI_ENDINGS = 10;
    static constexpr char LiEndings[NUM_LI_ENDINGS] = {'c', 'd', 'e', 'g', 'h', 'k', 'm', 'n', 'r', 't'};
    static constexpr int NUM_NON_SHORT_CONSONANTS = 3;
    static constexpr char NonShortConsonants[NUM_NON_SHORT_CONSONANTS] = {'w', 'x', 'Y'};
    static constexpr int NUM_MALE_WORDS = 9;
    const char *MaleWords[NUM_MALE_WORDS] = {"he", "him", "his", "himself", "man", "men", "boy", "husband", "actor"};
    static constexpr int NUM_FEMALE_WORDS = 8;
    const char *FemaleWords[NUM_FEMALE_WORDS] = {"she", "her", "herself", "woman", "women", "girl", "wife", "actress"};
    static constexpr int NUM_COMMON_WORDS = 12;
    const char *CommonWords[NUM_COMMON_WORDS] = {"the", "be", "to", "of", "and", "in", "that", "you", "have", "with", "from", "but"};
    static constexpr int NUM_SUFFIXES_STEP0 = 3;
    const char *SuffixesStep0[NUM_SUFFIXES_STEP0] = {"'s'", "'s", "'"};
    static constexpr int NUM_SUFFIXES_STEP1b = 6;
    const char *SuffixesStep1b[NUM_SUFFIXES_STEP1b] = {"eedly", "eed", "ed", "edly", "ing", "ingly"};
    static constexpr uint32_t TypesStep1b[NUM_SUFFIXES_STEP1b] = {English::AdverbOfManner, 0, English::PastTense,
                                                                  English::AdverbOfManner | English::PastTense, English::PresentParticiple,
                                                                  English::AdverbOfManner | English::PresentParticiple};
    static constexpr int NUM_SUFFIXES_STEP2 = 22;
    const char *(SuffixesStep2[NUM_SUFFIXES_STEP2])[2] = {{"ization", "ize"},
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
    const uint32_t TypesStep2[NUM_SUFFIXES_STEP2] = {English::SuffixION, English::SuffixION | English::SuffixAL, English::SuffixNESS,
                                                     English::SuffixNESS, English::SuffixNESS, English::SuffixION | English::SuffixAL,
                                                     English::AdverbOfManner, English::AdverbOfManner | English::SuffixITY,
                                                     English::AdverbOfManner, English::SuffixION, 0, English::SuffixITY,
                                                     English::AdverbOfManner, English::AdverbOfManner, English::SuffixITY, 0, 0,
                                                     English::AdverbOfManner, 0, 0, English::AdverbOfManner, English::AdverbOfManner};
    static constexpr int NUM_SUFFIXES_STEP3 = 8;
    const char *(SuffixesStep3[NUM_SUFFIXES_STEP3])[2] = {{"ational", "ate"},
                                                          {"tional",  "tion"},
                                                          {"alize",   "al"},
                                                          {"icate",   "ic"},
                                                          {"iciti",   "ic"},
                                                          {"ical",    "ic"},
                                                          {"ful",     ""},
                                                          {"ness",    ""}};
    static constexpr uint32_t TypesStep3[NUM_SUFFIXES_STEP3] = {English::SuffixION | English::SuffixAL,
                                                                English::SuffixION | English::SuffixAL, 0, 0, English::SuffixITY,
                                                                English::SuffixAL, English::AdjectiveFull, English::SuffixNESS};
    static constexpr int NUM_SUFFIXES_STEP4 = 20;
    const char *SuffixesStep4[NUM_SUFFIXES_STEP4] = {"al", "ance", "ence", "er", "ic", "able", "ible", "ant", "ement", "ment", "ent", "ou",
                                                     "ism", "ate", "iti", "ous", "ive", "ize", "sion", "tion"};
    static constexpr uint32_t TypesStep4[NUM_SUFFIXES_STEP4] = {English::SuffixAL, English::SuffixNCE, English::SuffixNCE, 0,
                                                                English::SuffixIC, English::SuffixCapable, English::SuffixCapable,
                                                                English::SuffixNT, 0, 0, English::SuffixNT, 0, 0, 0, English::SuffixITY,
                                                                English::SuffixOUS, English::SuffixIVE, 0, English::SuffixION,
                                                                English::SuffixION};
    static constexpr int NUM_EXCEPTION_REGION1 = 3;
    const char *ExceptionsRegion1[NUM_EXCEPTION_REGION1] = {"gener", "arsen", "commun"};
    static constexpr int NUM_EXCEPTIONS1 = 19;
    const char *(Exceptions1[NUM_EXCEPTIONS1])[2] = {{"skis",   "ski"},
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
    static constexpr uint32_t TypesExceptions1[NUM_EXCEPTIONS1] = {English::Noun | English::Plural,
                                                                   English::Noun | English::Plural | English::Verb,
                                                                   English::PresentParticiple, English::PresentParticiple,
                                                                   English::PresentParticiple, English::AdverbOfManner,
                                                                   English::AdverbOfManner, English::Adjective,
                                                                   English::Adjective | English::AdverbOfManner, 0, English::AdverbOfManner,
                                                                   English::Noun, English::Noun, 0, English::Noun, English::Noun,
                                                                   English::Noun, English::Noun | English::Plural, English::Noun};
    static constexpr int NUM_EXCEPTIONS2 = 8;
    const char *Exceptions2[NUM_EXCEPTIONS2] = {"inning", "outing", "canning", "herring", "earring", "proceed", "exceed", "succeed"};
    static constexpr uint32_t TypesExceptions2[NUM_EXCEPTIONS2] = {English::Noun, English::Noun, English::Noun, English::Noun,
                                                                   English::Noun, English::Verb, English::Verb, English::Verb};

    inline bool isConsonant(const char c) {
      return !isVowel(c);
    }

    static inline bool isShortConsonant(const char c) {
      return !charInArray(c, NonShortConsonants, NUM_NON_SHORT_CONSONANTS);
    }

    static inline bool isDouble(const char c) {
      return charInArray(c, Doubles, NUM_DOUBLES);
    }

    static inline bool isLiEnding(const char c) {
      return charInArray(c, LiEndings, NUM_LI_ENDINGS);
    }

    uint32_t getRegion1(const Word *w) {
      for( int i = 0; i < NUM_EXCEPTION_REGION1; i++ ) {
        if( w->startsWith(ExceptionsRegion1[i]))
          return uint32_t(strlen(ExceptionsRegion1[i]));
      }
      return getRegion(w, 0);
    }

    bool endsInShortSyllable(const Word *W) {
      if( W->end == W->start )
        return false;
      else if( W->end == W->start + 1 )
        return isVowel((*W)(1)) && isConsonant((*W)(0));
      else
        return (isConsonant((*W)(2)) && isVowel((*W)(1)) && isConsonant((*W)(0)) && isShortConsonant((*W)(0)));
    }

    bool isShortWord(const Word *W) {
      return (endsInShortSyllable(W) && getRegion1(W) == W->length());
    }

    inline bool hasVowels(const Word *W) {
      for( int i = W->start; i <= W->end; i++ ) {
        if( isVowel(W->letters[i]))
          return true;
      }
      return false;
    }

    static bool trimApostrophes(Word *W) {
      bool result = false;
      //trim all apostrophes from the beginning
      int cnt = 0;
      while( W->start != W->end && (*W)[0] == APOSTROPHE ) {
        result = true;
        W->start++;
        cnt++;
      }
      //trim the same number of apostrophes from the end (if there are)
      while( W->start != W->end && (*W)(0) == APOSTROPHE ) {
        if( cnt == 0 )
          break;
        W->end--;
        cnt--;
      }
      return result;
    }

    void markYsAsConsonants(Word *W) {
      if((*W)[0] == 'y' )
        W->letters[W->start] = 'Y';
      for( int i = W->start + 1; i <= W->end; i++ ) {
        if( isVowel(W->letters[i - 1]) && W->letters[i] == 'y' )
          W->letters[i] = 'Y';
      }
    }

    static bool processPrefixes(Word *W) {
      if( W->startsWith("irr") && W->length() > 5 && ((*W)[3] == 'a' || (*W)[3] == 'e'))
        W->start += 2, W->type |= English::Negation;
      else if( W->startsWith("over") && W->length() > 5 )
        W->start += 4, W->type |= English::PrefixOver;
      else if( W->startsWith("under") && W->length() > 6 )
        W->start += 5, W->type |= English::PrefixUnder;
      else if( W->startsWith("unn") && W->length() > 5 )
        W->start += 2, W->type |= English::Negation;
      else if( W->startsWith("non") && W->length() > (uint32_t) (5 + ((*W)[3] == '-')))
        W->start += 2 + ((*W)[3] == '-'), W->type |= English::Negation;
      else
        return false;
      return true;
    }

    bool processSuperlatives(Word *W) {
      if( W->endsWith("est") && W->length() > 4 ) {
        uint8_t i = W->end;
        W->end -= 3;
        W->type |= English::AdjectiveSuperlative;

        if((*W)(0) == (*W)(1) && (*W)(0) != 'r' && !(W->length() >= 4 && memcmp("sugg", &W->letters[W->end - 3], 4) == 0)) {
          W->end -= (((*W)(0) != 'f' && (*W)(0) != 'l' && (*W)(0) != 's') ||
                     (W->length() > 4 && (*W)(1) == 'l' && ((*W)(2) == 'u' || (*W)(3) == 'u' || (*W)(3) == 'v'))) &&
                    (!(W->length() == 3 && (*W)(1) == 'd' && (*W)(2) == 'o'));
          if( W->length() == 2 && ((*W)[0] != 'i' || (*W)[1] != 'n'))
            W->end = i, W->type &= ~English::AdjectiveSuperlative;
        } else {
          switch((*W)(0)) {
            case 'd':
            case 'k':
            case 'm':
            case 'y':
              break;
            case 'g': {
              if( !(W->length() > 3 && ((*W)(1) == 'n' || (*W)(1) == 'r') && memcmp("cong", &W->letters[W->end - 3], 4) != 0))
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              else
                W->end += ((*W)(2) == 'a');
              break;
            }
            case 'i': {
              W->letters[W->end] = 'y';
              break;
            }
            case 'l': {
              if( W->end == W->start + 1 || memcmp("mo", &W->letters[W->end - 2], 2) == 0 )
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              else
                W->end += isConsonant((*W)(1));
              break;
            }
            case 'n': {
              if( W->length() < 3 || isConsonant((*W)(1)) || isConsonant((*W)(2)))
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              break;
            }
            case 'r': {
              if( W->length() > 3 && isVowel((*W)(1)) && isVowel((*W)(2)))
                W->end += ((*W)(2) == 'u') && ((*W)(1) == 'a' || (*W)(1) == 'i');
              else
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              break;
            }
            case 's': {
              W->end++;
              break;
            }
            case 'w': {
              if( !(W->length() > 2 && isVowel((*W)(1))))
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              break;
            }
            case 'h': {
              if( !(W->length() > 2 && isConsonant((*W)(1))))
                W->end = i, W->type &= ~English::AdjectiveSuperlative;
              break;
            }
            default: {
              W->end += 3;
              W->type &= ~English::AdjectiveSuperlative;
            }
          }
        }
      }
      return (W->type & English::AdjectiveSuperlative) > 0;
    }

    //Search for the longest among the suffixes, 's' or 's or ' and remove if found.
    //Examples: Children's toys / Vice presidents' duties
    bool step0(Word *w) {
      for( int i = 0; i < NUM_SUFFIXES_STEP0; i++ ) {
        if( w->endsWith(SuffixesStep0[i])) {
          w->end -= uint8_t(strlen(SuffixesStep0[i]));
          w->type |= English::Plural;
          return true;
        }
      }
      return false;
    }

    //Search for the longest among the following suffixes, and perform the action indicated.
    bool step1A(Word *w) {
      //sses -> replace by ss
      if( w->endsWith("sses")) {
        w->end -= 2;
        w->type |= English::Plural;
        return true;
      }

      //ied+ ies* -> replace by i if preceded by more than one letter, otherwise by ie (so ties -> tie, cries -> cri)
      if( w->endsWith("ied") || w->endsWith("ies")) {
        w->type |= ((*w)(0) == 'd') ? English::PastTense : English::Plural;
        w->end -= w->length() > 4 ? 2 : 1;
        return true;
      }

      //us+ ss -> do nothing
      if( w->endsWith("us") || w->endsWith("ss"))
        return false;

      //s -> delete if the preceding word part contains a vowel not immediately before the s (so gas and this retain the s, gaps and kiwis lose it)
      if((*w)(0) == 's' && w->length() > 2 ) {
        for( int i = w->start; i <= w->end - 2; i++ ) {
          if( isVowel(w->letters[i])) {
            w->end--;
            w->type |= English::Plural;
            return true;
          }
        }
      }

      if( w->endsWith("n't") && w->length() > 4 ) {
        switch((*w)(3)) {
          case 'a': {
            if((*w)(4) == 'c' )
              w->end -= 2; // can't -> can
            else
              w->changeSuffix("n't", "ll"); // shan't -> shall
            break;
          }
          case 'i': {
            w->changeSuffix("in't", "m");
            break;
          } // ain't -> am
          case 'o': {
            if((*w)(4) == 'w' )
              w->changeSuffix("on't", "ill"); // won't -> will
            else
              w->end -= 3; //don't -> do
            break;
          }
          default:
            w->end -= 3; // couldn't -> could, shouldn't -> should, needn't -> need, hasn't -> has
        }
        w->type |= English::Negation;
        return true;
      }

      if( w->endsWith("hood") && w->length() > 7 ) {
        w->end -= 4; //brotherhood -> brother
        return true;
      }
      return false;
    }

    //Search for the longest among the following suffixes, and perform the action indicated.
    bool step1B(Word *W, const uint32_t R1) {
      for( int i = 0; i < NUM_SUFFIXES_STEP1b; i++ ) {
        if( W->endsWith(SuffixesStep1b[i])) {
          switch( i ) {
            case 0:
            case 1: {
              if( suffixInRn(W, R1, SuffixesStep1b[i]))
                W->end -= 1 + i * 2;
              break;
            }
            default: {
              uint8_t j = W->end;
              W->end -= uint8_t(strlen(SuffixesStep1b[i]));
              if( hasVowels(W)) {
                if( W->endsWith("at") || W->endsWith("bl") || W->endsWith("iz") || isShortWord(W))
                  (*W) += 'e';
                else if( W->length() > 2 ) {
                  if((*W)(0) == (*W)(1) && isDouble((*W)(0)))
                    W->end--;
                  else if( i == 2 || i == 3 ) {
                    switch((*W)(0)) {
                      case 'c':
                      case 's':
                      case 'v': {
                        W->end += !(W->endsWith("ss") || W->endsWith("ias"));
                        break;
                      }
                      case 'd': {
                        static constexpr char nAllowed[4] = {'a', 'e', 'i', 'o'};
                        W->end += isVowel((*W)(1)) && (!charInArray((*W)(2), nAllowed, 4));
                        break;
                      }
                      case 'k': {
                        W->end += W->endsWith("uak");
                        break;
                      }
                      case 'l': {
                        static constexpr char allowed1[10] = {'b', 'c', 'd', 'f', 'g', 'k', 'p', 't', 'y', 'z'};
                        static constexpr char allowed2[4] = {'a', 'i', 'o', 'u'};
                        W->end += charInArray((*W)(1), allowed1, 10) || (charInArray((*W)(1), allowed2, 4) && isConsonant((*W)(2)));
                        break;
                      }
                    }
                  } else if( i >= 4 ) {
                    switch((*W)(0)) {
                      case 'd': {
                        if( isVowel((*W)(1)) && (*W)(2) != 'a' && (*W)(2) != 'e' && (*W)(2) != 'o' )
                          (*W) += 'e';
                        break;
                      }
                      case 'g': {
                        static constexpr char Allowed[7] = {'a', 'd', 'e', 'i', 'l', 'r', 'u'};
                        if( charInArray((*W)(1), Allowed, 7) || ((*W)(1) == 'n' &&
                                                                 ((*W)(2) == 'e' || ((*W)(2) == 'u' && (*W)(3) != 'b' && (*W)(3) != 'd') ||
                                                                  ((*W)(2) == 'a' &&
                                                                   ((*W)(3) == 'r' || ((*W)(3) == 'h' && (*W)(4) == 'c'))) ||
                                                                  (W->endsWith("ring") && ((*W)(4) == 'c' || (*W)(4) == 'f')))))
                          (*W) += 'e';
                        break;
                      }
                      case 'l': {
                        if( !((*W)(1) == 'l' || (*W)(1) == 'r' || (*W)(1) == 'w' || (isVowel((*W)(1)) && isVowel((*W)(2)))))
                          (*W) += 'e';
                        if( W->endsWith("uell") && W->length() > 4 && (*W)(4) != 'q' )
                          W->end--;
                        break;
                      }
                      case 'r': {
                        if((((*W)(1) == 'i' && (*W)(2) != 'a' && (*W)(2) != 'e' && (*W)(2) != 'o') ||
                            ((*W)(1) == 'a' && (!((*W)(2) == 'e' || (*W)(2) == 'o' || ((*W)(2) == 'l' && (*W)(3) == 'l')))) ||
                            ((*W)(1) == 'o' && (!((*W)(2) == 'o' || ((*W)(2) == 't' && (*W)(3) != 's')))) || (*W)(1) == 'c' ||
                            (*W)(1) == 't') && (!W->endsWith("str")))
                          (*W) += 'e';
                        break;
                      }
                      case 't': {
                        if((*W)(1) == 'o' && (*W)(2) != 'g' && (*W)(2) != 'l' && (*W)(2) != 'i' && (*W)(2) != 'o' )
                          (*W) += 'e';
                        break;
                      }
                      case 'u': {
                        if( !(W->length() > 3 && isVowel((*W)(1)) && isVowel((*W)(2))))
                          (*W) += 'e';
                        break;
                      }
                      case 'z': {
                        if( W->endsWith("izz") && W->length() > 3 && ((*W)(3) == 'h' || (*W)(3) == 'u'))
                          W->end--;
                        else if((*W)(1) != 't' && (*W)(1) != 'z' )
                          (*W) += 'e';
                        break;
                      }
                      case 'k': {
                        if( W->endsWith("uak"))
                          (*W) += 'e';
                        break;
                      }
                      case 'b':
                      case 'c':
                      case 's':
                      case 'v': {
                        if( !(((*W)(0) == 'b' && ((*W)(1) == 'm' || (*W)(1) == 'r')) || W->endsWith("ss") || W->endsWith("ias") ||
                              (*W) == "zinc"))
                          (*W) += 'e';
                        break;
                      }
                    }
                  }
                }
              } else {
                W->end = j;
                return false;
              }
            }
          }
          W->type |= TypesStep1b[i];
          return true;
        }
      }
      return false;
    }

    bool step1C(Word *W) {
      if( W->length() > 2 && tolower((*W)(0)) == 'y' && isConsonant((*W)(1))) {
        W->letters[W->end] = 'i';
        return true;
      }
      return false;
    }

    bool step2(Word *W, const uint32_t R1) {
      for( int i = 0; i < NUM_SUFFIXES_STEP2; i++ ) {
        if( W->endsWith(SuffixesStep2[i][0]) && suffixInRn(W, R1, SuffixesStep2[i][0])) {
          W->changeSuffix(SuffixesStep2[i][0], SuffixesStep2[i][1]);
          W->type |= TypesStep2[i];
          return true;
        }
      }
      if( W->endsWith("logi") && suffixInRn(W, R1, "ogi")) {
        W->end--;
        return true;
      } else if( W->endsWith("li")) {
        if( suffixInRn(W, R1, "li") && isLiEnding((*W)(2))) {
          W->end -= 2;
          W->type |= English::AdverbOfManner;
          return true;
        } else if( W->length() > 3 ) {
          switch((*W)(2)) {
            case 'b': {
              W->letters[W->end] = 'e';
              W->type |= English::AdverbOfManner;
              return true;
            }
            case 'i': {
              if( W->length() > 4 ) {
                W->end -= 2;
                W->type |= English::AdverbOfManner;
                return true;
              }
              break;
            }
            case 'l': {
              if( W->length() > 5 && ((*W)(3) == 'a' || (*W)(3) == 'u')) {
                W->end -= 2;
                W->type |= English::AdverbOfManner;
                return true;
              }
              break;
            }
            case 's': {
              W->end -= 2;
              W->type |= English::AdverbOfManner;
              return true;
            }
            case 'e':
            case 'g':
            case 'm':
            case 'n':
            case 'r':
            case 'w': {
              if( W->length() > (uint32_t) (4 + ((*W)(2) == 'r'))) {
                W->end -= 2;
                W->type |= English::AdverbOfManner;
                return true;
              }
            }
          }
        }
      }
      return false;
    }

    bool step3(Word *w, const uint32_t r1, const uint32_t r2) {
      bool res = false;
      for( int i = 0; i < NUM_SUFFIXES_STEP3; i++ ) {
        if( w->endsWith(SuffixesStep3[i][0]) && suffixInRn(w, r1, SuffixesStep3[i][0])) {
          w->changeSuffix(SuffixesStep3[i][0], SuffixesStep3[i][1]);
          w->type |= TypesStep3[i];
          res = true;
          break;
        }
      }
      if( w->endsWith("ative") && suffixInRn(w, r2, "ative")) {
        w->end -= 5;
        w->type |= English::SuffixIVE;
        return true;
      }
      if( w->length() > 5 && w->endsWith("less")) {
        w->end -= 4;
        w->type |= English::AdjectiveWithout;
        return true;
      }
      return res;
    }

    //Search for the longest among the following suffixes, and, if found and in r2, perform the action indicated.
    bool step4(Word *w, const uint32_t r2) {
      bool res = false;
      for( int i = 0; i < NUM_SUFFIXES_STEP4; i++ ) {
        if( w->endsWith(SuffixesStep4[i]) && suffixInRn(w, r2, SuffixesStep4[i])) {
          w->end -= uint8_t(strlen(SuffixesStep4[i]) - (i >
                                                        17) /*sion, tion*/); // remove: al   ance   ence   er   ic   able   ible   ant   ement   ment   ent   ism   ate   iti   ous   ive   ize ; ion -> delete if preceded by s or t
          if( !(i == 10 /* ent */ && (*w)(0) == 'm')) //exception: no TypesStep4 should be assigned for 'agreement'
            w->type |= TypesStep4[i];
          if( i == 0 && w->endsWith("nti")) {
            w->end--; // ntial -> nt
            res = true; // process ant+ial, ent+ial
            continue;
          }
          return true;
        }
      }
      return res;
    }

    //Search for the the following suffixes, and, if found, perform the action indicated.
    bool step5(Word *w, const uint32_t r1, const uint32_t r2) {
      if((*w)(0) == 'e' && (*w) != "here" ) {
        if( suffixInRn(w, r2, "e"))
          w->end--; //e -> delete if in r2, or in r1 and not preceded by a short syllable
        else if( suffixInRn(w, r1, "e")) {
          w->end--; //e -> delete if in r1 and not preceded by a short syllable
          w->end += endsInShortSyllable(w);
        } else
          return false;
        return true;
      } else if( w->length() > 1 && (*w)(0) == 'l' && suffixInRn(w, r2, "l") && (*w)(1) == 'l' ) {
        w->end--; //l -> delete if in r2 and preceded by l
        return true;
      }
      return false;
    }

public:
    inline bool isVowel(const char c) final {
      return charInArray(c, Vowels, NUM_VOWELS);
    }

    bool stem(Word *w) override {
      if( w->length() < 2 ) {
        w->calculateStemHash();
        //w->print(); //for debugging
        return false;
      }
      bool res = trimApostrophes(w);
      res |= processPrefixes(w);
      res |= processSuperlatives(w);
      for( int i = 0; i < NUM_EXCEPTIONS1; i++ ) {
        if((*w) == Exceptions1[i][0] ) {
          if( i < 11 ) {
            size_t len = strlen(Exceptions1[i][1]);
            memcpy(&w->letters[w->start], Exceptions1[i][1], len);
            w->end = w->start + uint8_t(len - 1);
          }
          w->calculateStemHash();
          w->type |= TypesExceptions1[i];
          w->language = Language::English;
          //w->print(); //for debugging
          return (i < 11);
        }
      }

      // start of modified Porter2 Stemmer
      markYsAsConsonants(w);
      uint32_t r1 = getRegion1(w), r2 = getRegion(w, r1);
      res |= step0(w);
      res |= step1A(w);
      for( int i = 0; i < NUM_EXCEPTIONS2; i++ ) {
        if((*w) == Exceptions2[i] ) {
          w->calculateStemHash();
          w->type |= TypesExceptions2[i];
          w->language = Language::English;
          //w->print(); //for debugging
          return res;
        }
      }
      res |= step1B(w, r1);
      res |= step1C(w);
      res |= step2(w, r1);
      res |= step3(w, r1, r2);
      res |= step4(w, r2);
      res |= step5(w, r1, r2);

      for( uint8_t i = w->start; i <= w->end; i++ ) {
        if( w->letters[i] == 'Y' )
          w->letters[i] = 'y';
      }
      if( !w->type || w->type == English::Plural ) {
        if( w->matchesAny(MaleWords, NUM_MALE_WORDS))
          res = true, w->type |= English::Male;
        else if( w->matchesAny(FemaleWords, NUM_FEMALE_WORDS))
          res = true, w->type |= English::Female;
      }
      if( !res )
        res = w->matchesAny(CommonWords, NUM_COMMON_WORDS);
      w->calculateStemHash();
      if( res )
        w->language = Language::English;
      //w->print(); //for debugging
      return res;
    }
};

#endif //PAQ8PX_ENGLISHSTEMMER_HPP
