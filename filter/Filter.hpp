#ifndef PAQ8PX_FILTER_HPP
#define PAQ8PX_FILTER_HPP

#include "../file/File.hpp"
#include "../Encoder.hpp"
#include "Filters.hpp"
#include <cstdint>

class Filter {
public:
    virtual void encode(File *in, File *out, uint64_t size, int info, int &headerSize) = 0;
    virtual uint64_t decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) = 0;
    virtual void setEncoder(Encoder &en) = 0;

    virtual ~Filter() = default;
};

#endif //PAQ8PX_FILTER_HPP
