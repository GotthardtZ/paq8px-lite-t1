#include "Stemmer.hpp"

auto Stemmer::getRegion(const Word *w, const uint32_t from) -> uint32_t {
  bool hasVowel = false;
  for( int i = w->start + from; i <= w->end; i++ ) {
    if( isVowel(w->letters[i])) {
      hasVowel = true;
      continue;
    }
    if( hasVowel ) {
      return i - w->start + 1;
    }
  }
  return w->start + w->length();
}

auto Stemmer::suffixInRn(const Word *w, const uint32_t rn, const char *suffix) -> bool {
  return (w->start != w->end && rn <= w->length() - strlen(suffix));
}

auto Stemmer::charInArray(const char c, const char *a, const int len) -> bool {
  if( a == nullptr ) {
    return false;
  }
  int i = 0;
  for( ; i < len && c != a[i]; i++ ) { ;
  }
  return i < len;
}
