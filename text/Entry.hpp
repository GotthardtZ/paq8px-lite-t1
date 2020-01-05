#ifndef PAQ8PX_ENTRY_HPP
#define PAQ8PX_ENTRY_HPP

#include <cstdint>

#pragma pack(push, 1)

struct Entry {
    short prefix;
    uint8_t suffix;
    bool termination;
    uint32_t embedding;
};
#pragma pack(pop)

#endif //PAQ8PX_ENTRY_HPP
