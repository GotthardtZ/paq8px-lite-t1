#ifndef PAQ8PX_SHARED_HPP
#define PAQ8PX_SHARED_HPP

#include <cstdint>
#include "RingBuffer.hpp"

// helper #defines to access shared variables
#define INJECT_SHARED_buf const RingBuffer<uint8_t> &buf = shared->buf;
#define INJECT_SHARED_pos const uint32_t pos = shared->buf.getpos();
#define INJECT_SHARED_y const uint8_t y = shared->y;
#define INJECT_SHARED_c0 const uint8_t c0 = shared->c0;
#define INJECT_SHARED_c1 const uint8_t c1 = shared->c1;
#define INJECT_SHARED_bpos const uint8_t bpos = shared->bitPosition;
#define INJECT_SHARED_c4 const uint32_t c4 = shared->c4;
#define INJECT_SHARED_c8 const uint32_t c8 = shared->c8;
#define INJECT_STATS_blpos const uint32_t blpos = stats->blpos;

/**
 * Global context available to all models.
 */
struct Shared {
public:
    RingBuffer<uint8_t> buf; // Rotating input queue set by Predictor
    uint8_t y = 0; // Last bit, 0 or 1
    uint8_t c0 = 1; // Last 0-7 bits of the partial byte with a leading 1 bit (1-255)
    uint8_t c1 = 0; // Last whole byte, also c4&0xff or buf(1)
    uint8_t bitPosition = 0; // Bits in c0 (0 to 7), in other words the position of the bit to be predicted (0=MSB)
    uint32_t c4 = 0; // Last 4 whole bytes (buf(4)..buf(1)), packed.  Last byte is bits 0-7.
    uint32_t c8 = 0; // Another 4 bytes (buf(8)..buf(5))
    uint8_t options;

    static Shared *getInstance();
    void update();
    void reset();
    void copyTo(Shared *dst);
private:
    Shared() {};  // Private so that it can  not be called
    Shared(Shared const &) {};             // copy constructor is private
    Shared &operator=(Shared const &) {return *this;};  // assignment operator is private
    static Shared *mPInstance;

};

#endif //PAQ8PX_SHARED_HPP
