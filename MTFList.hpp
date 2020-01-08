#ifndef PAQ8PX_MTFLIST_HPP
#define PAQ8PX_MTFLIST_HPP

#include <cstdint>
#include "Array.hpp"

/**
 * Move To Front List
 */
class MTFList {
private:
    int root, Index;
    Array<int, 16> previous;
    Array<int, 16> next;
public:
    MTFList(uint16_t n);
    int getFirst();
    int getNext();
    void moveToFront(int i);
};

#endif //PAQ8PX_MTFLIST_HPP
