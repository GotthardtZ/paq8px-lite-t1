#include "Word.hpp"

auto Word::calculateHash() -> uint64_t {
  uint64_t h = 0;
  for( int i = start; i <= end; i++ ) {
    h = combine64(h, letters[i]);
  }
  return h;
}

Word::Word() { reset(); }

void Word::reset() {
  memset(&letters[0], 0, sizeof(uint8_t) * MAX_WORD_SIZE);
  start = end = 0;
  Hash[0] = Hash[1] = 0;
  type = language = embedding = 0;
}

auto Word::operator==(const char *s) const -> bool {
  size_t len = strlen(s);
  return (static_cast<size_t>(end - start + static_cast<int>(letters[start] != 0)) == len && memcmp(&letters[start], s, len) == 0);
}

auto Word::operator!=(const char *s) const -> bool {
  return !operator==(s);
}

void Word::operator+=(const char c) {
  if( end < MAX_WORD_SIZE - 1 ) {
    end += static_cast<int>(letters[end] > 0);
    letters[end] = tolower(c);
  }
}

auto Word::operator-(const Word w) const -> uint32_t {
  uint32_t res = 0;
  for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 ) {
    res = (res << 8U) | uint8_t(uint8_t(embedding >> j) - uint8_t(w.embedding >> j));
  }
  return res;
}

auto Word::operator+(const Word w) const -> uint32_t {
  uint32_t res = 0;
  for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 ) {
    res = (res << 8U) | uint8_t(uint8_t(embedding >> j) + uint8_t(w.embedding >> j));
  }
  return res;
}

auto Word::operator[](uint8_t i) const -> uint8_t {
  return (end - start >= i) ? letters[start + i] : 0;
}

auto Word::operator()(uint8_t i) const -> uint8_t {
  return (end - start >= i) ? letters[end - i] : 0;
}

auto Word::length() const -> uint32_t {
  if( letters[start] != 0 ) {
    return end - start + 1;
  }
  return 0;
}

auto Word::distanceTo(const Word w) const -> uint32_t {
  uint32_t res = 0;
  for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 ) {
    res += square(abs(int(uint8_t(embedding >> j) - uint8_t(w.embedding >> j))));
  }
  return static_cast<uint32_t>(sqrt(res));
}

void Word::calculateWordHash() {
  Hash[1] = Hash[0] = calculateHash(); // Hash[1]: placeholder for stem hash, will be overwritten after stemming
}

void Word::calculateStemHash() {
  Hash[1] = calculateHash();
}

auto Word::changeSuffix(const char *oldSuffix, const char *newSuffix) -> bool {
  size_t len = strlen(oldSuffix);
  if( length() > len && memcmp(&letters[end - len + 1], oldSuffix, len) == 0 ) {
    size_t n = strlen(newSuffix);
    if( n > 0 ) {
      memcpy(&letters[end - int(len) + 1], newSuffix, min(MAX_WORD_SIZE - 1, end + int(n)) - end);
      end = min(MAX_WORD_SIZE - 1, end - int(len) + int(n));
    } else {
      end -= uint8_t(len);
    }
    return true;
  }
  return false;
}

auto Word::matchesAny(const char **a, const int count) -> bool {
  int i = 0;
  auto len = static_cast<size_t>(length());
  for( ; i < count && (len != strlen(a[i]) || memcmp(&letters[start], a[i], len) != 0); i++ ) { ;
  }
  return i < count;
}

auto Word::endsWith(const char *suffix) const -> bool {
  size_t len = strlen(suffix);
  return (length() > len && memcmp(&letters[end - len + 1], suffix, len) == 0);
}

auto Word::startsWith(const char *prefix) const -> bool {
  size_t len = strlen(prefix);
  return (length() > len && memcmp(&letters[start], prefix, len) == 0);
}

void Word::print() const {
  for( int r = start; r <= end; r++ ) {
    printf("%c", static_cast<char>(letters[r]));
  }
  printf("\n");
}
