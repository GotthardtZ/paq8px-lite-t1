#include "TextParserStateInfo.hpp"

TextParserStateInfo::TextParserStateInfo() : _start(1), _end(1), _EOLType(1) {}

void TextParserStateInfo::reset(uint64_t startPos) {
  _start.resize(1);
  _end.resize(1);
  _start[0] = startPos;
  _end[0] = startPos - 1;
  _EOLType.resize(1);
  _EOLType[0] = 0;
  invalidCount = 0;
  UTF8State = 0;
}

uint64_t TextParserStateInfo::start() { return _start[_start.size() - 1]; }

uint64_t TextParserStateInfo::end() { return _end[_end.size() - 1]; }

void TextParserStateInfo::setEnd(uint64_t end) { _end[_end.size() - 1] = end; }

uint32_t TextParserStateInfo::eolType() { return _EOLType[_EOLType.size() - 1]; }

void TextParserStateInfo::setEolType(uint32_t eolType) { _EOLType[_EOLType.size() - 1] = eolType; }

uint64_t TextParserStateInfo::validLength() { return end() - start() + 1; }

void TextParserStateInfo::next(uint64_t startPos) {
  _start.pushBack(startPos);
  _end.pushBack(startPos - 1);
  _EOLType.pushBack(0);
  invalidCount = 0;
  UTF8State = 0;
}

void TextParserStateInfo::removeFirst() {
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
