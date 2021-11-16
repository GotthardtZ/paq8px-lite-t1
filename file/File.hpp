#ifndef PAQ8PX_FILE_HPP
#define PAQ8PX_FILE_HPP

#include "../String.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

/**
 * This is an abstract class for all the required file operations.
 */
class File {
public:
    virtual ~File() = default;
    virtual auto open(const char *filename, bool mustSucceed) -> bool = 0;
    virtual void create(const char *filename) = 0;
    virtual void close() = 0;
    virtual auto getchar() -> int = 0;
    virtual void putChar(uint8_t c) = 0;
    void append(const char *s);
    auto getVLI() -> uint64_t;
    void putVLI(uint64_t i);
    virtual void setpos(uint64_t newPos) = 0;
    virtual void setEnd() = 0;
    virtual auto curPos() -> uint64_t = 0;
    virtual auto eof() -> bool = 0;
};

#endif //PAQ8PX_FILE_HPP
