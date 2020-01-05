#ifndef PAQ8PX_STEMMER_HPP
#define PAQ8PX_STEMMER_HPP

class Stemmer {
protected:
    uint32_t getRegion(const Word *w, const uint32_t from) {
      bool hasVowel = false;
      for( int i = w->start + from; i <= w->end; i++ ) {
        if( isVowel(w->letters[i])) {
          hasVowel = true;
          continue;
        } else if( hasVowel )
          return i - w->start + 1;
      }
      return w->start + w->length();
    }

    static bool suffixInRn(const Word *w, const uint32_t rn, const char *suffix) {
      return (w->start != w->end && rn <= w->length() - strlen(suffix));
    }

public:
    virtual ~Stemmer() = default;;
    virtual bool isVowel(char c) = 0;
    virtual bool stem(Word *w) = 0;
};

#endif //PAQ8PX_STEMMER_HPP
