#ifndef PAQ8PX_FILEDISK_HPP
#define PAQ8PX_FILEDISK_HPP

#include "File.hpp"
#include "fileUtils.hpp"

/**
 * This class is responsible for files on disk.
 * It simply passes function calls to stdio.
 */
class FileDisk : public File {
protected:
    FILE *file;

public:
    FileDisk();
    ~FileDisk() override;
    auto open(const char *filename, bool mustSucceed) -> bool override;
    void create(const char *filename) override;
    void close() override;
    auto getchar() -> int override;
    void putChar(uint8_t c) override;
    void setpos(uint64_t newPos) override;
    void setEnd() override;
    auto curPos() -> uint64_t override;
    auto eof() -> bool override;
};

#endif //PAQ8PX_FILEDISK_HPP
