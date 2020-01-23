#ifndef PAQ8PX_SHARED_HPP
#define PAQ8PX_SHARED_HPP

#include "RingBuffer.hpp"
#include "UpdateBroadcaster.hpp"
#include <cstdint>

// helper #defines to access shared variables
#define INJECT_SHARED_buf const RingBuffer<uint8_t> &buf = shared->buf;
#define INJECT_SHARED_y const uint8_t y = shared->y;

/**
 * Global context available to all models.
 */
struct Shared {
public:
    RingBuffer<uint8_t> buf; /**< Rotating input queue set by Predictor */
    uint8_t y = 0; /**< Last bit, 0 or 1 */
    uint8_t c0 = 1; /**< Last 0-7 bits of the partial byte with a leading 1 bit (1-255) */
    uint8_t c1 = 0; /**< Last whole byte, also c4&0xff or buf(1) */
    uint8_t bitPosition = 0; /**< Bits in c0 (0 to 7), in other words the position of the bit to be predicted (0=MSB) */
    uint32_t c4 = 0; /**< Last 4 whole bytes (buf(4)..buf(1)), packed.  Last byte is bits 0-7. */
    uint32_t c8 = 0; /**< Another 4 bytes (buf(8)..buf(5)) */
    uint8_t options = 0;
    SIMD chosenSimd = SIMD_NONE; /**< default value, will be overridden by the CPU dispatcher, and may be overridden from the command line */
    uint8_t level = 0;
    uint64_t mem = 0;
    bool toScreen = true;
    UpdateBroadcaster *updateBroadcaster = UpdateBroadcaster::getInstance();

    static auto getInstance() -> Shared *;
    void update();
    void reset();
    void setLevel(uint8_t l);
private:
    Shared() = default;

    /**
     * Copy constructor is private so that it cannot be called
     */
    Shared(Shared const & /*unused*/) {}

    /**
     * Assignment operator is private so that it cannot be called
     */
    auto operator=(Shared const & /*unused*/) -> Shared & { return *this; }

    /**
     * Determine if output is redirected
     * @return
     */
    static auto isOutputDirected() -> bool;

    static Shared *mPInstance;
};

#endif //PAQ8PX_SHARED_HPP
