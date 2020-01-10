#include "GermanStemmer.hpp"

void GermanStemmer::convertUtf8(Word *w) {
  for( int i = w->start; i < w->end; i++ ) {
    uint8_t c = w->letters[i + 1] + ((w->letters[i + 1] < 0x9F) ? 0x60 : 0x40);
    if( w->letters[i] == 0xC3 && (isVowel(c) || c == 0xDF)) {
      w->letters[i] = c;
      if( i + 1 < w->end )
        memmove(&w->letters[i + 1], &w->letters[i + 2], w->end - i - 1);
      w->end--;
    }
  }
}

void GermanStemmer::replaceSharpS(Word *w) {
  for( int i = w->start; i <= w->end; i++ ) {
    if( w->letters[i] == 0xDF ) {
      w->letters[i] = 's';
      if( i < MAX_WORD_SIZE - 1 ) {
        memmove(&w->letters[i + 2], &w->letters[i + 1], MAX_WORD_SIZE - i - 2);
        w->letters[i + 1] = 's';
        w->end += (w->end < MAX_WORD_SIZE - 1);
      }
    }
  }
}

void GermanStemmer::markVowelsAsConsonants(Word *w) {
  for( int i = w->start + 1; i < w->end; i++ ) {
    uint8_t c = w->letters[i];
    if((c == 'u' || c == 'y') && isVowel(w->letters[i - 1]) && isVowel(w->letters[i + 1]))
      w->letters[i] = toupper(c);
  }
}

bool GermanStemmer::isValidEnding(const char c, const bool includeR) {
  return charInArray(c, endings, NUM_ENDINGS) || (includeR && c == 'r');
}

bool GermanStemmer::step1(Word *w, const uint32_t r1) {
  int i = 0;
  for( ; i < 3; i++ ) {
    if( w->endsWith(suffixesStep1[i]) && suffixInRn(w, r1, suffixesStep1[i])) {
      w->end -= uint8_t(strlen(suffixesStep1[i]));
      return true;
    }
  }
  for( ; i < NUM_SUFFIXES_STEP1; i++ ) {
    if( w->endsWith(suffixesStep1[i]) && suffixInRn(w, r1, suffixesStep1[i])) {
      w->end -= uint8_t(strlen(suffixesStep1[i]));
      w->end -= uint8_t(w->endsWith("niss"));
      return true;
    }
  }
  if( w->endsWith("s") && suffixInRn(w, r1, "s") && isValidEnding((*w)(1), true)) {
    w->end--;
    return true;
  }
  return false;
}

bool GermanStemmer::step2(Word *w, const uint32_t r1) {
  for( int i = 0; i < NUM_SUFFIXES_STEP2; i++ ) {
    if( w->endsWith(suffixesStep2[i]) && suffixInRn(w, r1, suffixesStep2[i])) {
      w->end -= uint8_t(strlen(suffixesStep2[i]));
      return true;
    }
  }
  if( w->endsWith("st") && suffixInRn(w, r1, "st") && w->length() > 5 && isValidEnding((*w)(2))) {
    w->end -= 2;
    return true;
  }
  return false;
}

bool GermanStemmer::step3(Word *w, const uint32_t r1, const uint32_t r2) {
  int i = 0;
  for( ; i < 2; i++ ) {
    if( w->endsWith(suffixesStep3[i]) && suffixInRn(w, r2, suffixesStep3[i])) {
      w->end -= uint8_t(strlen(suffixesStep3[i]));
      if( w->endsWith("ig") && (*w)(2) != 'e' && suffixInRn(w, r2, "ig"))
        w->end -= 2;
      if( i )
        w->type |= German::Noun;
      return true;
    }
  }
  for( ; i < 5; i++ ) {
    if( w->endsWith(suffixesStep3[i]) && suffixInRn(w, r2, suffixesStep3[i]) && (*w)((uint8_t) strlen(suffixesStep3[i])) != 'e' ) {
      w->end -= uint8_t(strlen(suffixesStep3[i]));
      if( i > 2 )
        w->type |= German::Adjective;
      return true;
    }
  }
  for( ; i < NUM_SUFFIXES_STEP3; i++ ) {
    if( w->endsWith(suffixesStep3[i]) && suffixInRn(w, r2, suffixesStep3[i])) {
      w->end -= uint8_t(strlen(suffixesStep3[i]));
      if((w->endsWith("er") || w->endsWith("en")) && suffixInRn(w, r1, "e?"))
        w->end -= 2;
      if( i > 5 )
        w->type |= German::Noun | German::Female;
      return true;
    }
  }
  if( w->endsWith("keit") && suffixInRn(w, r2, "keit")) {
    w->end -= 4;
    if( w->endsWith("lich") && suffixInRn(w, r2, "lich"))
      w->end -= 4;
    else if( w->endsWith("ig") && suffixInRn(w, r2, "ig"))
      w->end -= 2;
    w->type |= German::Noun | German::Female;
    return true;
  }
  return false;
}

bool GermanStemmer::isVowel(const char c) {
  return charInArray(c, vowels, NUM_VOWELS);
}

bool GermanStemmer::stem(Word *w) {
  convertUtf8(w);
  if( w->length() < 2 ) {
    w->calculateStemHash();
    return false;
  }
  replaceSharpS(w);
  markVowelsAsConsonants(w);
  uint32_t r1 = getRegion(w, 0), r2 = getRegion(w, r1);
  r1 = min(3, r1);
  bool res = step1(w, r1);
  res |= step2(w, r1);
  res |= step3(w, r1, r2);
  for( int i = w->start; i <= w->end; i++ ) {
    switch( w->letters[i] ) {
      case 0xE4: {
        w->letters[i] = 'a';
        break;
      }
      case 0xF6:
      case 0xFC: {
        w->letters[i] -= 0x87;
        break;
      }
      default:
        w->letters[i] = tolower(w->letters[i]);
    }
  }
  if( !res )
    res = w->matchesAny(commonWords, NUM_COMMON_WORDS);
  w->calculateStemHash();
  if( res )
    w->language = Language::German;
  return res;
}
