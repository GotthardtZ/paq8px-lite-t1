#ifndef PAQ8PX_ADAPTIVEMAP_HPP
#define PAQ8PX_ADAPTIVEMAP_HPP

/**
 * This is the base class for StateMap and APM.
 * Purpose: common members are here
 */
class AdaptiveMap : protected IPredictor {
protected:
    const Shared *const shared;
    Array<uint32_t> t; // cxt -> prediction in high 22 bits, count in low 10 bits
    int limit;
    int *dt; // Pointer to division table
    AdaptiveMap(const Shared *const sh, const int n, const int lim) : shared(sh), t(n), limit(lim) {
      dt = DivisionTable::getDT();
    }

    ~AdaptiveMap() override = default;

    void update(uint32_t *const p) {
      uint32_t p0 = p[0];
      const int n = p0 & 1023U; //count
      const int pr = p0 >> 10U; //prediction (22-bit fractional part)
      if( n < limit )
        ++p0;
      else
        p0 = (p0 & 0xfffffc00U) | limit;
      INJECT_SHARED_y
      const int target = y << 22U; //(22-bit fractional part)
      const int delta = ((target - pr) >> 3U) * dt[n]; //the larger the count (n) the less it should adapt pr+=(target-pr)/(n+1.5)
      p0 += delta & 0xfffffc00U;
      p[0] = p0;
    }

public:
    void setLimit(const int lim) { limit = lim; };
};

#endif //PAQ8PX_ADAPTIVEMAP_HPP
