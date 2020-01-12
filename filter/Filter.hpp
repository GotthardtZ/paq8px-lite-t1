#ifndef PAQ8PX_FILTER_HPP
#define PAQ8PX_FILTER_HPP

#include "../file/File.hpp"
#include "../Encoder.hpp"
#include <cstdint>

typedef enum {
    FDECOMPRESS, FCOMPARE, FDISCARD
} FMode;

class Filter {
public:
    virtual void encode(File *in, File *out, uint64_t size, int info, int &headerSize) = 0;
    virtual auto decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) -> uint64_t = 0;
    virtual void setEncoder(Encoder &en) = 0;

    virtual ~Filter() = default;
};

#endif //PAQ8PX_FILTER_HPP
