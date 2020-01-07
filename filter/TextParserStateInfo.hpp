#ifndef PAQ8PX_TEXTPARSERSTATEINFO_HPP
#define PAQ8PX_TEXTPARSERSTATEINFO_HPP

#define TEXT_MIN_SIZE 1500 // size of minimum allowed text block (in bytes)
#define TEXT_MAX_MISSES 2 // threshold: max allowed number of invalid UTF8 sequences seen recently before reporting "fail"
#define TEXT_ADAPT_RATE 256 // smaller (like 32) = illegal sequences are allowed to come more often, larger (like 1024) = more rigorous detection

struct TextParserStateInfo {
    Array<uint64_t> _start;
    Array<uint64_t> _end; // position of last char with a valid UTF8 state: marks the end of the detected TEXT block
    Array<uint32_t> _EOLType; // 0: none or CR-only;   1: CRLF-only (applicable to EOL transform);   2: mixed or LF-only
    uint32_t invalidCount; // adaptive count of invalid UTF8 sequences seen recently
    uint32_t UTF8State; // state of utf8 parser; 0: valid;  12: invalid;  any other value: yet uncertain (more bytes must be read)
    TextParserStateInfo() : _start(0), _end(0), _EOLType(0) {}

    void reset(uint64_t startPos) {
      _start.resize(1);
      _end.resize(1);
      _start[0] = startPos;
      _end[0] = startPos - 1;
      _EOLType.resize(1);
      _EOLType[0] = 0;
      invalidCount = 0;
      UTF8State = 0;
    }

    uint64_t start() { return _start[_start.size() - 1]; }

    uint64_t end() { return _end[_end.size() - 1]; }

    void setEnd(uint64_t end) { _end[_end.size() - 1] = end; }

    uint32_t eolType() { return _EOLType[_EOLType.size() - 1]; }

    void setEolType(uint32_t eolType) { _EOLType[_EOLType.size() - 1] = eolType; }

    uint64_t validLength() { return end() - start() + 1; }

    void next(uint64_t startPos) {
      _start.pushBack(startPos);
      _end.pushBack(startPos - 1);
      _EOLType.pushBack(0);
      invalidCount = 0;
      UTF8State = 0;
    }

    void removeFirst() {
      if( _start.size() == 1 )
        reset(0);
      else {
        for( int i = 0; i < (int) _start.size() - 1; i++ ) {
          _start[i] = _start[i + 1];
          _end[i] = _end[i + 1];
          _EOLType[i] = _EOLType[i + 1];
        }
        _start.popBack();
        _end.popBack();
        _EOLType.popBack();
      }
    }
} textParser;


#endif //PAQ8PX_TEXTPARSERSTATEINFO_HPP
