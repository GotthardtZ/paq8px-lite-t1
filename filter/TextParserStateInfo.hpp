#ifndef PAQ8PX_TEXTPARSERSTATEINFO_HPP
#define PAQ8PX_TEXTPARSERSTATEINFO_HPP

#include "../Array.hpp"
#include <cstdint>

#define TEXT_MIN_SIZE 1500 /**< size of minimum allowed text block (in bytes) */
#define TEXT_MAX_MISSES 2 /**< threshold: max allowed number of invalid UTF8 sequences seen recently before reporting "fail" */
#define TEXT_ADAPT_RATE 256 /**< smaller (like 32) = illegal sequences are allowed to come more often, larger (like 1024) = more rigorous detection */

struct TextParserStateInfo {
    Array<uint64_t> _start;
    Array<uint64_t> _end; /**< position of last char with a valid UTF8 state: marks the end of the detected TEXT block */
    Array<uint32_t> _EOLType; /**< 0: none or CR-only;   1: CRLF-only (applicable to EOL transform);   2: mixed or LF-only */
    uint32_t invalidCount {}; /**< adaptive count of invalid UTF8 sequences seen recently */
    uint32_t UTF8State {}; /**< state of utf8 parser; 0: valid;  12: invalid;  any other value: yet uncertain (more bytes must be read) */
    TextParserStateInfo();
    void reset(uint64_t startPos);
    auto start() -> uint64_t;
    auto end() -> uint64_t;
    void setEnd(uint64_t end);
    auto eolType() -> uint32_t;
    void setEolType(uint32_t eolType);
    auto validLength() -> uint64_t;
    void next(uint64_t startPos);
    void removeFirst();
};

#endif //PAQ8PX_TEXTPARSERSTATEINFO_HPP
