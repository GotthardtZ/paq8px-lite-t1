#ifndef PAQ8PX_WORD_HPP
#define PAQ8PX_WORD_HPP

#define MAX_WORD_SIZE 64
#define WORD_EMBEDDING_SIZE 3

class Word {
private:
    uint64_t calculateHash() {
      uint64_t h = 0;
      for( int i = start; i <= end; i++ )
        h = combine64(h, letters[i]);
      return h;
    }

public:
    uint8_t letters[MAX_WORD_SIZE];
    uint8_t start, end;
    uint64_t Hash[2];
    uint32_t type, language, embedding;

    Word() { reset(); }

    void reset() {
      memset(&letters[0], 0, sizeof(uint8_t) * MAX_WORD_SIZE);
      start = end = 0;
      Hash[0] = Hash[1] = 0;
      type = language = embedding = 0;
    }

    bool operator==(const char *s) const {
      size_t len = strlen(s);
      return ((size_t) (end - start + (letters[start] != 0)) == len && memcmp(&letters[start], s, len) == 0);
    }

    bool operator!=(const char *s) const {
      return !operator==(s);
    }

    void operator+=(const char c) {
      if( end < MAX_WORD_SIZE - 1 ) {
        end += (letters[end] > 0);
        letters[end] = tolower(c);
      }
    }

    uint32_t operator-(const Word w) const {
      uint32_t res = 0;
      for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 )
        res = (res << 8U) | uint8_t(uint8_t(embedding >> j) - uint8_t(w.embedding >> j));
      return res;
    }

    uint32_t operator+(const Word w) const {
      uint32_t res = 0;
      for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 )
        res = (res << 8U) | uint8_t(uint8_t(embedding >> j) + uint8_t(w.embedding >> j));
      return res;
    }

    uint8_t operator[](uint8_t i) const {
      return (end - start >= i) ? letters[start + i] : 0;
    }

    uint8_t operator()(uint8_t i) const {
      return (end - start >= i) ? letters[end - i] : 0;
    }

    [[nodiscard]] constexpr uint32_t length() const {
      if( letters[start] != 0 )
        return end - start + 1;
      return 0;
    }

    [[nodiscard]] uint32_t distanceTo(const Word w) const {
      uint32_t res = 0;
      for( int i = 0, j = 0; i < WORD_EMBEDDING_SIZE; i++, j += 8 )
        res += square(abs(int(uint8_t(embedding >> j) - uint8_t(w.embedding >> j))));
      return (uint32_t) sqrt(res);
    }

    void calculateWordHash() {
      Hash[1] = Hash[0] = calculateHash(); // Hash[1]: placeholder for stem hash, will be overwritten after stemming
    }

    void calculateStemHash() { // called by a stemmer after stemming
      Hash[1] = calculateHash();
    }

    bool changeSuffix(const char *oldSuffix, const char *newSuffix) {
      size_t len = strlen(oldSuffix);
      if( length() > len && memcmp(&letters[end - len + 1], oldSuffix, len) == 0 ) {
        size_t n = strlen(newSuffix);
        if( n > 0 ) {
          memcpy(&letters[end - int(len) + 1], newSuffix, min(MAX_WORD_SIZE - 1, end + int(n)) - end);
          end = min(MAX_WORD_SIZE - 1, end - int(len) + int(n));
        } else
          end -= uint8_t(len);
        return true;
      }
      return false;
    }

    bool matchesAny(const char **a, const int count) {
      int i = 0;
      auto len = (size_t) length();
      for( ; i < count && (len != strlen(a[i]) || memcmp(&letters[start], a[i], len) != 0); i++ );
      return i < count;
    }

    bool endsWith(const char *suffix) const {
      size_t len = strlen(suffix);
      return (length() > len && memcmp(&letters[end - len + 1], suffix, len) == 0);
    }

    bool startsWith(const char *prefix) const {
      size_t len = strlen(prefix);
      return (length() > len && memcmp(&letters[start], prefix, len) == 0);
    }

    void print() const {
      for( int r = start; r <= end; r++ )
        printf("%c", (char) letters[r]);
      printf("\n");
    }
};

#endif //PAQ8PX_WORD_HPP
