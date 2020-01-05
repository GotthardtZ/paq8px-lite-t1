#ifndef PAQ8PX_FRENCHSTEMMER_HPP
#define PAQ8PX_FRENCHSTEMMER_HPP

// French suffix stemmer, based on the Porter stemmer.

class FrenchStemmer : public Stemmer {
private:
    static constexpr int NUM_VOWELS = 17;
    static constexpr char Vowels[NUM_VOWELS] = {'a', 'e', 'i', 'o', 'u', 'y', '\xE2', '\xE0', '\xEB', '\xE9', '\xEA',
                                                '\xE8', '\xEF', '\xEE', '\xF4', '\xFB', '\xF9'};
    static constexpr int NUM_COMMON_WORDS = 10;
    const char *CommonWords[NUM_COMMON_WORDS] = {"de", "la", "le", "et", "en", "un", "une", "du", "que", "pas"};
    static constexpr int NUM_EXCEPTIONS = 3;
    const char *(Exceptions[NUM_EXCEPTIONS])[2] = {{"monument", "monument"},
                                                   {"yeux",     "oeil"},
                                                   {"travaux",  "travail"},};
    static constexpr uint32_t TypesExceptions[NUM_EXCEPTIONS] = {French::Noun, French::Noun | French::Plural,
                                                                 French::Noun | French::Plural};
    static constexpr int NUM_SUFFIXES_STEP1 = 39;
    const char *SuffixesStep1[NUM_SUFFIXES_STEP1] = {"ance", "iqUe", "isme", "able", "iste", "eux", "ances", "iqUes",
                                                     "ismes", "ables", "istes", //11
                                                     "atrice", "ateur", "ation", "atrices", "ateurs", "ations", //6
                                                     "logie", "logies", //2
                                                     "usion", "ution", "usions", "utions", //4
                                                     "ence", "ences", //2
                                                     "issement", "issements", //2
                                                     "ement", "ements", //2
                                                     "it\xE9", "it\xE9s", //2
                                                     "if", "ive", "ifs", "ives", //4
                                                     "euse", "euses", //2
                                                     "ment", "ments" //2
    };
    static constexpr int NUM_SUFFIXES_STEP2a = 35;
    const char *SuffixesStep2a[NUM_SUFFIXES_STEP2a] = {"issaIent", "issantes", "iraIent", "issante", "issants",
                                                       "issions", "irions", "issais", "issait", "issant", "issent",
                                                       "issiez", "issons", "irais", "irait", "irent", "iriez", "irons",
                                                       "iront", "isses", "issez", "\xEEmes", "\xEEtes", "irai", "iras",
                                                       "irez", "isse", "ies", "ira", "\xEEt", "ie", "ir", "is", "it",
                                                       "i"};
    static constexpr int NUM_SUFFIXES_STEP2b = 38;
    const char *SuffixesStep2b[NUM_SUFFIXES_STEP2b] = {"eraIent", "assions", "erions", "assent", "assiez", "\xE8rent",
                                                       "erais", "erait", "eriez", "erons", "eront", "aIent", "antes",
                                                       "asses", "ions", "erai", "eras", "erez", "\xE2mes", "\xE2tes",
                                                       "ante", "ants", "asse", "\xE9"
                                                                               "es", "era", "iez", "ais", "ait", "ant",
                                                       "\xE9"
                                                       "e", "\xE9s", "er", "ez", "\xE2t", "ai", "as", "\xE9", "a"};
    static constexpr int NUM_SET_STEP4 = 6;
    static constexpr char SetStep4[NUM_SET_STEP4] = {'a', 'i', 'o', 'u', '\xE8', 's'};
    static constexpr int NUM_SUFFIXES_STEP4 = 7;
    const char *SuffixesStep4[NUM_SUFFIXES_STEP4] = {"i\xE8re", "I\xE8re", "ion", "ier", "Ier", "e", "\xEB"};
    static constexpr int NUM_SUFFIXES_STEP5 = 5;
    const char *SuffixesStep5[NUM_SUFFIXES_STEP5] = {"enn", "onn", "ett", "ell", "eill"};

    inline bool isConsonant(const char c) {
      return !isVowel(c);
    }

    void ConvertUTF8(Word *W) {
      for( int i = W->start; i < W->end; i++ ) {
        uint8_t c = W->letters[i + 1] + ((W->letters[i + 1] < 0xA0) ? 0x60 : 0x40);
        if( W->letters[i] == 0xC3 && (isVowel(c) || (W->letters[i + 1] & 0xDF) == 0x87)) {
          W->letters[i] = c;
          if( i + 1 < W->end )
            memmove(&W->letters[i + 1], &W->letters[i + 2], W->end - i - 1);
          W->end--;
        }
      }
    }

    void markVowelsAsConsonants(Word *w) {
      for( int i = w->start; i <= w->end; i++ ) {
        switch( w->letters[i] ) {
          case 'i':
          case 'u': {
            if( i > w->start && i < w->end &&
                (isVowel(w->letters[i - 1]) || (w->letters[i - 1] == 'q' && w->letters[i] == 'u')) &&
                isVowel(w->letters[i + 1]))
              w->letters[i] = toupper(w->letters[i]);
            break;
          }
          case 'y': {
            if((i > w->start && isVowel(w->letters[i - 1])) || (i < w->end && isVowel(w->letters[i + 1])))
              w->letters[i] = toupper(w->letters[i]);
          }
        }
      }
    }

    uint32_t getRv(Word *w) {
      uint32_t len = w->length(), res = w->start + len;
      if( len >= 3 && ((isVowel(w->letters[w->start]) && isVowel(w->letters[w->start + 1])) || w->startsWith("par") ||
                       w->startsWith("col") || w->startsWith("tap")))
        return w->start + 3;
      else {
        for( int i = w->start + 1; i <= w->end; i++ ) {
          if( isVowel(w->letters[i]))
            return i + 1;
        }
      }
      return res;
    }

    bool step1(Word *w, const uint32_t rv, const uint32_t r1, const uint32_t r2, bool *forceStep2A) {
      int i = 0;
      for( ; i < 11; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r2, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          if( i == 3 /*able*/)
            w->type |= French::Adjective;
          return true;
        }
      }
      for( ; i < 17; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r2, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          if( w->endsWith("ic"))
            w->changeSuffix("c", "qU");
          return true;
        }
      }
      for( ; i < 25; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r2, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i])) - 1 - (i < 19) * 2;
          if( i > 22 ) {
            w->end += 2;
            w->letters[w->end] = 't';
          }
          return true;
        }
      }
      for( ; i < 27; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r1, SuffixesStep1[i]) &&
            isConsonant((*w)((uint8_t) strlen(SuffixesStep1[i])))) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          return true;
        }
      }
      for( ; i < 29; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, rv, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          if( w->endsWith("iv") && suffixInRn(w, r2, "iv")) {
            w->end -= 2;
            if( w->endsWith("at") && suffixInRn(w, r2, "at"))
              w->end -= 2;
          } else if( w->endsWith("eus")) {
            if( suffixInRn(w, r2, "eus"))
              w->end -= 3;
            else if( suffixInRn(w, r1, "eus"))
              w->letters[w->end] = 'x';
          } else if((w->endsWith("abl") && suffixInRn(w, r2, "abl")) ||
                    (w->endsWith("iqU") && suffixInRn(w, r2, "iqU")))
            w->end -= 3;
          else if((w->endsWith("i\xE8r") && suffixInRn(w, rv, "i\xE8r")) ||
                  (w->endsWith("I\xE8r") && suffixInRn(w, rv, "I\xE8r"))) {
            w->end -= 2;
            w->letters[w->end] = 'i';
          }
          return true;
        }
      }
      for( ; i < 31; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r2, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          if( w->endsWith("abil")) {
            if( suffixInRn(w, r2, "abil"))
              w->end -= 4;
            else
              w->end--, w->letters[w->end] = 'l';
          } else if( w->endsWith("ic")) {
            if( suffixInRn(w, r2, "ic"))
              w->end -= 2;
            else
              w->changeSuffix("c", "qU");
          } else if( w->endsWith("iv") && suffixInRn(w, r2, "iv"))
            w->end -= 2;
          return true;
        }
      }
      for( ; i < 35; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, r2, SuffixesStep1[i])) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          if( w->endsWith("at") && suffixInRn(w, r2, "at")) {
            w->end -= 2;
            if( w->endsWith("ic")) {
              if( suffixInRn(w, r2, "ic"))
                w->end -= 2;
              else
                w->changeSuffix("c", "qU");
            }
          }
          return true;
        }
      }
      for( ; i < 37; i++ ) {
        if( w->endsWith(SuffixesStep1[i])) {
          if( suffixInRn(w, r2, SuffixesStep1[i])) {
            w->end -= uint8_t(strlen(SuffixesStep1[i]));
            return true;
          } else if( suffixInRn(w, r1, SuffixesStep1[i])) {
            w->changeSuffix(SuffixesStep1[i], "eux");
            return true;
          }
        }
      }
      for( ; i < NUM_SUFFIXES_STEP1; i++ ) {
        if( w->endsWith(SuffixesStep1[i]) && suffixInRn(w, rv + 1, SuffixesStep1[i]) &&
            isVowel((*w)((uint8_t) strlen(SuffixesStep1[i])))) {
          w->end -= uint8_t(strlen(SuffixesStep1[i]));
          (*forceStep2A) = true;
          return true;
        }
      }
      if( w->endsWith("eaux") || (*w) == "eaux" ) {
        w->end--;
        w->type |= French::Plural;
        return true;
      } else if( w->endsWith("aux") && suffixInRn(w, r1, "aux")) {
        w->end--, w->letters[w->end] = 'l';
        w->type |= French::Plural;
        return true;
      } else if( w->endsWith("amment") && suffixInRn(w, rv, "amment")) {
        w->changeSuffix("amment", "ant");
        (*forceStep2A) = true;
        return true;
      } else if( w->endsWith("emment") && suffixInRn(w, rv, "emment")) {
        w->changeSuffix("emment", "ent");
        (*forceStep2A) = true;
        return true;
      }
      return false;
    }

    bool step2A(Word *w, const uint32_t rv) {
      for( int i = 0; i < NUM_SUFFIXES_STEP2a; i++ ) {
        if( w->endsWith(SuffixesStep2a[i]) && suffixInRn(w, rv + 1, SuffixesStep2a[i]) &&
            isConsonant((*w)((uint8_t) strlen(SuffixesStep2a[i])))) {
          w->end -= uint8_t(strlen(SuffixesStep2a[i]));
          if( i == 31 /*ir*/)
            w->type |= French::Verb;
          return true;
        }
      }
      return false;
    }

    bool step2B(Word *W, const uint32_t rv, const uint32_t r2) {
      for( int i = 0; i < NUM_SUFFIXES_STEP2b; i++ ) {
        if( W->endsWith(SuffixesStep2b[i]) && suffixInRn(W, rv, SuffixesStep2b[i])) {
          switch( SuffixesStep2b[i][0] ) {
            case 'a':
            case '\xE2': {
              W->end -= uint8_t(strlen(SuffixesStep2b[i]));
              if( W->endsWith("e") && suffixInRn(W, rv, "e"))
                W->end--;
              return true;
            }
            default: {
              if( i != 14 || suffixInRn(W, r2, SuffixesStep2b[i])) {
                W->end -= uint8_t(strlen(SuffixesStep2b[i]));
                return true;
              }
            }
          }
        }
      }
      return false;
    }

    static void step3(Word *w) {
      char *final = (char *) &w->letters[w->end];
      if((*final) == 'Y' )
        (*final) = 'i';
      else if((*final) == '\xE7' )
        (*final) = 'c';
    }

    bool step4(Word *W, const uint32_t rv, const uint32_t r2) {
      bool res = false;
      if( W->length() >= 2 && W->letters[W->end] == 's' && !charInArray((*W)(1), SetStep4, NUM_SET_STEP4)) {
        W->end--;
        res = true;
      }
      for( int i = 0; i < NUM_SUFFIXES_STEP4; i++ ) {
        if( W->endsWith(SuffixesStep4[i]) && suffixInRn(W, rv, SuffixesStep4[i])) {
          switch( i ) {
            case 2: { //ion
              char prec = (*W)(3);
              if( suffixInRn(W, r2, SuffixesStep4[i]) && suffixInRn(W, rv + 1, SuffixesStep4[i]) &&
                  (prec == 's' || prec == 't')) {
                W->end -= 3;
                return true;
              }
              break;
            }
            case 5: { //e
              W->end--;
              return true;
            }
            case 6: { //\xEB
              if( W->endsWith("gu\xEB")) {
                W->end--;
                return true;
              }
              break;
            }
            default: {
              W->changeSuffix(SuffixesStep4[i], "i");
              return true;
            }
          }
        }
      }
      return res;
    }

    bool step5(Word *w) {
      for( int i = 0; i < NUM_SUFFIXES_STEP5; i++ ) {
        if( w->endsWith(SuffixesStep5[i])) {
          w->end--;
          return true;
        }
      }
      return false;
    }

    bool step6(Word *w) {
      for( int i = w->end; i >= w->start; i-- ) {
        if( isVowel(w->letters[i])) {
          if( i < w->end && (w->letters[i] & 0xFE) == 0xE8 ) {
            w->letters[i] = 'e';
            return true;
          }
          return false;
        }
      }
      return false;
    }

public:
    inline bool isVowel(const char c) final {
      return charInArray(c, Vowels, NUM_VOWELS);
    }

    bool stem(Word *w) override {
      ConvertUTF8(w);
      if( w->length() < 2 ) {
        w->calculateStemHash();
        return false;
      }
      for( int i = 0; i < NUM_EXCEPTIONS; i++ ) {
        if((*w) == Exceptions[i][0] ) {
          size_t len = strlen(Exceptions[i][1]);
          memcpy(&w->letters[w->start], Exceptions[i][1], len);
          w->end = w->start + uint8_t(len - 1);
          w->calculateStemHash();
          w->type |= TypesExceptions[i];
          w->language = Language::French;
          return true;
        }
      }
      markVowelsAsConsonants(w);
      uint32_t rv = getRv(w), r1 = getRegion(w, 0), r2 = getRegion(w, r1);
      bool doNextStep = false, res = step1(w, rv, r1, r2, &doNextStep);
      doNextStep |= !res;
      if( doNextStep ) {
        doNextStep = !step2A(w, rv);
        res |= !doNextStep;
        if( doNextStep )
          res |= step2B(w, rv, r2);
      }
      if( res )
        step3(w);
      else
        res |= step4(w, rv, r2);
      res |= step5(w);
      res |= step6(w);
      for( int i = w->start; i <= w->end; i++ )
        w->letters[i] = tolower(w->letters[i]);
      if( !res )
        res = w->matchesAny(CommonWords, NUM_COMMON_WORDS);
      w->calculateStemHash();
      if( res )
        w->language = Language::French;
      return res;
    }
};

#endif //PAQ8PX_FRENCHSTEMMER_HPP
