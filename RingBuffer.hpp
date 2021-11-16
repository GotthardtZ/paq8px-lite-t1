#ifndef PAQ8PX_RINGBUFFER_HPP
#define PAQ8PX_RINGBUFFER_HPP

#include <cassert>
#include <cstdint>
#include "Array.hpp"
#include "Utils.hpp"

template<class T>
class RingBuffer {
private:
    Array<T> b;
    uint32_t offset {0}; /**< Number of input bytes in buffer (not wrapped), will be masked when used for indexing */
    uint32_t mask;

public:
    /**
     * Creates an array of @ref size bytes (must be a power of 2).
     * @param size number of bytes in array
     */
    explicit RingBuffer(const uint32_t size = 0) : b(size), mask(size - 1) {
      assert(isPowerOf2(size));
    }

    void setSize(uint32_t newSize) {
      assert(newSize > 0 && isPowerOf2(newSize));
      b.resize(newSize);
      offset = 0;
      mask = newSize - 1;
    }

    [[nodiscard]] auto getpos() const -> uint32_t {
      return offset;
    }

    void fill(const T B) {
      const auto n = (uint32_t) b.size();
      for( uint32_t i = 0; i < n; i++ ) {
        b[i] = B;
      }
    }

    void add(const T B) {
      b[offset & mask] = B;
      offset++;
    }

    /**
     * Returns a reference to the i'th byte with wrap (no out of bounds).
     * @param i
     * @return
     */
    auto operator[](const uint32_t i) const -> T {
      return b[i & mask];
    }

    void set(const uint32_t i, const T B) {
      b[i & mask] = B;
    }

    /**
     * Returns i'th byte back from pos (i>0) with wrap (no out of bounds)
     * @param i
     * @return
     */
    auto operator()(const uint32_t i) const -> T {
      //assert(i!=0);
      return b[(offset - i) & mask];
    }

    void reset() {
      fill(0);
      offset = 0;
    }

    /**
     * @return the size of the RingBuffer
     */
    auto size() -> uint32_t {
      return (uint32_t) b.size();
    }

};

#endif //PAQ8PX_RINGBUFFER_HPP
