#include "IndirectModel.hpp"

IndirectModel::IndirectModel(const Shared* const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM) {}

void IndirectModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_buf
    INJECT_SHARED_c4
    uint32_t d = c4 & 0xffffU;
    uint32_t c = d & 255U;
    uint32_t d2 = (buf(1) & 31U) + 32 * (buf(2) & 31U) + 1024 * (buf(3) & 31U);
    uint32_t d3 = (buf(1) >> 3 & 31) + 32 * (buf(3) >> 3U & 31U) + 1024 * (buf(4) >> 3U & 31U);
    uint32_t &r1 = t1[d >> 8U];
    r1 = r1 << 8U | c;
    uint16_t &r2 = t2[c4 >> 8 & 0xffff];
    r2 = r2 << 8U | c;
    uint16_t &r3 = t3[(buf(2) & 31) + 32 * (buf(3) & 31U) + 1024 * (buf(4) & 31)];
    r3 = r3 << 8U | c;
    uint16_t &r4 = t4[(buf(2) >> 3 & 31) + 32 * (buf(4) >> 3U & 31U) + 1024 * (buf(5) >> 3U & 31U)];
    r4 = r4 << 8U | c;
    const uint32_t t = c | t1[c] << 8U;
    const uint32_t t0 = d | t2[d] << 16U;
    const uint32_t ta = d2 | t3[d2] << 16U;
    const uint32_t tc = d3 | t4[d3] << 16U;
    const uint8_t pc = tolower(uint8_t(c4 >> 8U));
    iCtx += (c = tolower(c)), iCtx = (pc << 8U) | c;
    const uint32_t ctx0 = iCtx();
    const uint32_t mask = static_cast<int>(uint8_t(t1[c]) == uint8_t(t2[d])) | (static_cast<int>(uint8_t(t1[c]) == uint8_t(t3[d2])) << 1U) |
                          (static_cast<int>(uint8_t(t1[c]) == uint8_t(t4[d3])) << 2U) |
                          (static_cast<int>(uint8_t(t1[c]) == uint8_t(ctx0)) << 3U);
    uint64_t i = 0;
    cm.set(hash(++i, t));
    cm.set(hash(++i, t0));
    cm.set(hash(++i, ta));
    cm.set(hash(++i, tc));
    cm.set(hash(++i, t & 0xff00U, mask));
    cm.set(hash(++i, t0 & 0xff0000U));
    cm.set(hash(++i, ta & 0xff0000U));
    cm.set(hash(++i, tc & 0xff0000U));
    cm.set(hash(++i, t & 0xffffU));
    cm.set(hash(++i, t0 & 0xffffffU));
    cm.set(hash(++i, ta & 0xffffffU));
    cm.set(hash(++i, tc & 0xffffffU));
    cm.set(hash(++i, ctx0 & 0xffU, c));
    cm.set(hash(++i, ctx0 & 0xffffU));
    cm.set(hash(++i, ctx0 & 0x7f7fffU));
  }
  cm.mix(m);
}
