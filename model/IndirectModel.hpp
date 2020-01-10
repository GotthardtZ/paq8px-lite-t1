#ifndef PAQ8PX_INDIRECTMODEL_HPP
#define PAQ8PX_INDIRECTMODEL_HPP

#include <cctype>
#include "../Shared.hpp"
#include "../RingBuffer.hpp"

/**
 * The context is a byte string history that occurs within a 1 or 2 byte context.
 */
class IndirectModel {
private:
    static constexpr int nCM = 15;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); // 75
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;

private:
    const Shared *const shared;
    ContextMap cm;
    Array<uint32_t> t1 {256};
    Array<uint16_t> t2 {0x10000};
    Array<uint16_t> t3 {0x8000};
    Array<uint16_t> t4 {0x8000};
    IndirectContext<uint32_t> iCtx {16, 8}; /* BitsPerContext, InputBits */
public:
    IndirectModel(const Shared *const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM) {}

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        INJECT_SHARED_c4
        INJECT_SHARED_buf
        uint32_t d = c4 & 0xffff, c = d & 255, d2 = (buf(1) & 31) + 32 * (buf(2) & 31) + 1024 * (buf(3) & 31);
        uint32_t d3 = (buf(1) >> 3 & 31) + 32 * (buf(3) >> 3 & 31) + 1024 * (buf(4) >> 3 & 31);
        uint32_t &r1 = t1[d >> 8];
        r1 = r1 << 8 | c;
        uint16_t &r2 = t2[c4 >> 8 & 0xffff];
        r2 = r2 << 8 | c;
        uint16_t &r3 = t3[(buf(2) & 31) + 32 * (buf(3) & 31) + 1024 * (buf(4) & 31)];
        r3 = r3 << 8 | c;
        uint16_t &r4 = t4[(buf(2) >> 3 & 31) + 32 * (buf(4) >> 3 & 31) + 1024 * (buf(5) >> 3 & 31)];
        r4 = r4 << 8 | c;
        const uint32_t t = c | t1[c] << 8;
        const uint32_t t0 = d | t2[d] << 16;
        const uint32_t ta = d2 | t3[d2] << 16;
        const uint32_t tc = d3 | t4[d3] << 16;
        const uint8_t pc = tolower(uint8_t(c4 >> 8));
        iCtx += (c = tolower(c)), iCtx = (pc << 8) | c;
        const uint32_t ctx0 = iCtx(), mask = (uint8_t(t1[c]) == uint8_t(t2[d])) | ((uint8_t(t1[c]) == uint8_t(t3[d2])) << 1) |
                                             ((uint8_t(t1[c]) == uint8_t(t4[d3])) << 2) | ((uint8_t(t1[c]) == uint8_t(ctx0)) << 3);
        uint64_t i = 0;
        cm.set(hash(++i, t));
        cm.set(hash(++i, t0));
        cm.set(hash(++i, ta));
        cm.set(hash(++i, tc));
        cm.set(hash(++i, t & 0xff00, mask));
        cm.set(hash(++i, t0 & 0xff0000));
        cm.set(hash(++i, ta & 0xff0000));
        cm.set(hash(++i, tc & 0xff0000));
        cm.set(hash(++i, t & 0xffff));
        cm.set(hash(++i, t0 & 0xffffff));
        cm.set(hash(++i, ta & 0xffffff));
        cm.set(hash(++i, tc & 0xffffff));
        cm.set(hash(++i, ctx0 & 0xff, c));
        cm.set(hash(++i, ctx0 & 0xffff));
        cm.set(hash(++i, ctx0 & 0x7f7fff));
      }
      cm.mix(m);
    }
};

#endif //PAQ8PX_INDIRECTMODEL_HPP
