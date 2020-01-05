#ifndef PAQ8PX_MTFLIST_HPP
#define PAQ8PX_MTFLIST_HPP

#include <cstdint>
#include "Array.hpp"

class MTFList {
private:
    int root, Index;
    Array<int, 16> previous;
    Array<int, 16> next;

public:
    MTFList(const uint16_t n) : root(0), Index(0), previous(n), next(n) {
      assert(n > 0);
      for( int i = 0; i < n; i++ ) {
        previous[i] = i - 1;
        next[i] = i + 1;
      }
      next[n - 1] = -1;
    }

    inline int getFirst() {
      return Index = root;
    }

    inline int getNext() {
      if( Index >= 0 )
        Index = next[Index];
      return Index;
    }

    inline void moveToFront(const int i) {
      assert(uint32_t(i) < Previous.size());
      if((Index = i) == root )
        return;
      const int p = previous[Index], n = next[Index];
      if( p >= 0 )
        next[p] = next[Index];
      if( n >= 0 )
        previous[n] = previous[Index];
      previous[root] = Index;
      next[Index] = root;
      root = Index;
      previous[root] = -1;
    }
};

#endif //PAQ8PX_MTFLIST_HPP
