#include "SSE.hpp"

SSE::SSE(Shared* const sh) : shared(sh),
  Text {
    { /*APM:*/  {sh,0x10000,24}, {sh,0x10000,24}, {sh,0x10000,24}, {sh,0x10000,24}}, /* APM: contexts, steps */
    { /*APM1:*/ {sh,0x10000,7}, {sh,0x10000,6}, {sh,0x10000,6}} /* APM1: contexts, rate */
  },
  Image {
    { /*APM:*/ {{sh,0x1000,24}, {sh,0x10000,24}, {sh,0x10000,24}, {sh,0x10000,24}}, /*APM1:*/ {{sh,0x10000,7}, {sh,0x10000,7}} }, // color
    { /*APM:*/ {{sh,0x1000,24}, {sh,0x10000,24}, {sh,0x10000,24}, {sh,0x10000,24}}, /*APM1:*/ {{sh,0x10000,5}, {sh,0x10000,6}} }, // palette
    { /*APM:*/ {{sh,0x1000,24}, {sh,0x10000,24}, {sh,0x10000,24}} } //gray
  },
  Jpeg{
    { /*APM:*/ {sh,0x2000,24} }
  },
  DEC{
    { /*APM:*/ {sh,25*26,20} }
  },
  x86_64{
    { /*APM:*/ {sh,0x800,20}, {sh,0x10000,16}, {sh,0x10000,16} }
  },
  Generic {
    /*APM1:*/ {{sh,0x2000,7}, {sh,0x10000,7}, {sh,0x10000,7}, {sh,0x10000,7}, {sh,0x10000,7}, {sh,0x10000,7}, {sh,0x10000,7}}
  }
{}
auto SSE::p(int pr_orig) -> int {
  INJECT_SHARED_c0
  INJECT_SHARED_bpos
  INJECT_SHARED_c4
  INJECT_SHARED_blockPos
  INJECT_SHARED_blockType
  uint32_t pr; // the final probability
  switch( blockType ) {
    case TEXT:
    case TEXT_EOL: {
      int limit = 0x03FF >> (static_cast<int>(blockPos < 0x0FFF) * 2);
      uint32_t pr0 = Text.APMs[0].p(pr_orig, static_cast<uint32_t>(c0) << 8 | (shared->State.Text.mask & 0x0F) | (shared->State.misses & 0x0F) << 4, limit);
      uint32_t pr1 = Text.APMs[1].p(pr_orig, finalize64(hash(bpos, shared->State.misses & 3, c4 & 0xFFFF, shared->State.Text.mask >> 4), 16), limit);
      uint32_t pr2 = Text.APMs[2].p(pr_orig, finalize64(hash(c0, shared->State.Match.expectedByte << 2 | shared->State.Match.length3), 16), limit);
      uint32_t pr3 = Text.APMs[3].p(pr_orig, finalize64(hash(c0, c4 & 0xFFFF, shared->State.Text.firstLetter), 16), limit);

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;

      uint32_t pr4 = Text.APM1s[0].p(prA, finalize64(hash(shared->State.Match.expectedByte << 2 | shared->State.Match.length3, c4 & 0xFF), 16));
      uint32_t pr5 = Text.APM1s[1].p(pr0, finalize64(hash(c0, c4 & 0x00FFFFFF), 16));
      uint32_t pr6 = Text.APM1s[2].p(pr0, finalize64(hash(c0, c4 & 0xFFFFFF00), 16));

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;
      pr = (prA + prB + 1) >> 1;
      break;
    }
    case IMAGE24:
    case IMAGE32: {
      int limit = 0x03FF >> (static_cast<int>(blockPos < 0x0FFF) * 4);
      uint32_t pr0 = Image.Color.APMs[0].p(pr_orig, static_cast<uint32_t>(c0) << 4 | (shared->State.misses & 0x0F), limit);
      uint32_t pr1 = Image.Color.APMs[1].p(pr_orig, finalize64(hash(c0, shared->State.Image.pixels.W, shared->State.Image.pixels.WW), 16), limit);
      uint32_t pr2 = Image.Color.APMs[2].p(pr_orig, finalize64(hash(c0, shared->State.Image.pixels.N, shared->State.Image.pixels.NN), 16), limit);
      uint32_t pr3 = Image.Color.APMs[3].p(pr_orig, (c0 << 8) | shared->State.Image.ctx, limit);

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      uint32_t pr4 = Image.Color.APM1s[0].p(pr0, finalize64(hash(c0, shared->State.Image.pixels.W, (c4 & 0xFF) - shared->State.Image.pixels.Wp1, shared->State.Image.plane), 16));
      uint32_t pr5 = Image.Color.APM1s[1].p(pr0, finalize64(hash(c0, shared->State.Image.pixels.N, (c4 & 0xFF) - shared->State.Image.pixels.Np1, shared->State.Image.plane), 16));

      uint32_t prB = (pr0 * 2 + pr4 * 3 + pr5 * 3 + 4) >> 3;
      pr = (prA + prB + 1) >> 1;
      break;
    }
    case IMAGE8GRAY: {
      int limit = 0x03FF >> (static_cast<int>(blockPos < 0x0FFF) * 4);
      uint32_t pr0 = Image.Gray.APMs[0].p(pr_orig, static_cast<uint32_t>(c0) << 4 | (shared->State.misses & 0x0F), limit);
      uint32_t pr1 = Image.Gray.APMs[1].p(pr0, (c0 << 8) | shared->State.Image.ctx, limit);
      uint32_t pr2 = Image.Gray.APMs[2].p(pr_orig, bpos | (shared->State.Image.ctx & 0xF8) | (shared->State.Match.expectedByte << 8), limit);

      int prA = (2 * pr_orig + pr1 + pr2 + 2) >> 2;
      pr = (pr0 + prA + 1) >> 1;
      break;
    }
    case IMAGE8: {
      int limit = 0x03FF >> (static_cast<int>(blockPos < 0x0FFF) * 4);
      uint32_t pr0 = Image.Palette.APMs[0].p(pr_orig, c0 | (shared->State.misses & 0x0F) << 8, limit);
      uint32_t pr1 = Image.Palette.APMs[1].p(pr_orig, finalize64(hash(c0 | shared->State.Image.pixels.W << 8 | shared->State.Image.pixels.N << 16), 16), limit);
      uint32_t pr2 = Image.Palette.APMs[2].p(pr_orig, finalize64(hash(c0 | shared->State.Image.pixels.N << 8 | shared->State.Image.pixels.NN << 16), 16), limit);
      uint32_t pr3 = Image.Palette.APMs[3].p(pr_orig, finalize64(hash(c0 | shared->State.Image.pixels.W << 8 | shared->State.Image.pixels.WW << 16), 16), limit);

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      uint32_t pr4 = Image.Palette.APM1s[0].p(prA, finalize64(hash(c0 | shared->State.Match.expectedByte << 8 | shared->State.Image.pixels.N << 16), 16));
      uint32_t pr5 = Image.Palette.APM1s[1].p(pr0, finalize64(hash(c0 | shared->State.Image.pixels.W << 8 | shared->State.Image.pixels.N << 16), 16));

      uint32_t prB = (pr0 * 2 + pr4 + pr5 + 2) >> 2;
      pr = (prA + prB + 1) >> 1;
      break;
    }
    case JPEG: {
      uint32_t pr0 = Jpeg.APMs[0].p(pr_orig, shared->State.JPEG.state, 0x3FF);
      pr = (pr0 + pr_orig + 1) / 2;
      break;
    }
    case DEC_ALPHA: {
      int const limit = 0x03FF >> (static_cast<int>(blockPos < 0x0FFF) * 4);
      uint32_t pr0 = DEC.APMs[0].p(pr_orig, (shared->State.DEC.state * 26u) + shared->State.DEC.bcount, limit);
      pr = (pr0 * 4 + pr_orig * 2 + 3) / 6;
      break;
    }
    case EXE: {
      int const limit = 0x03FF >> (static_cast<int>(blockPos < 0xFFF) * 4);
      uint32_t pr0 = x86_64.APMs[0].p(pr_orig, (shared->State.x86_64.state << 3) | bpos, limit);
      uint32_t pr1 = x86_64.APMs[1].p(pr_orig, (c0 << 8) | shared->State.x86_64.state, limit);
      uint32_t pr2 = x86_64.APMs[2].p((pr1+pr0+1)/2, finalize64(hash(c4 & 0xFF, bpos, shared->State.misses & 1, shared->State.x86_64.state >> 3), 16), limit);
      pr = (pr0 + pr_orig + pr1 + pr2 + 2) >> 2;
      break;
    }
    default: {
      uint32_t pr0 = Generic.APM1s[0].p(pr_orig, (shared->State.Match.length3) << 11 | static_cast<uint32_t>(c0) << 3| (shared->State.misses & 7));
      
      const uint16_t ctx1 = c0 | (c4 & 0xFF) << 8;
      const uint16_t ctx2 = c0 ^finalize64(hash(c4 & 0xFFFF), 16);
      const uint16_t ctx3 = c0 ^finalize64(hash(c4 & 0xFFFFFF), 16);

      uint32_t pr1 = Generic.APM1s[1].p(pr_orig, ctx1);
      uint32_t pr2 = Generic.APM1s[2].p(pr_orig, ctx2);
      uint32_t pr3 = Generic.APM1s[3].p(pr_orig, ctx3);

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      uint32_t pr4 = Generic.APM1s[4].p(pr0, (static_cast<uint32_t>(shared->State.Match.expectedByte) << 8) | (c4 & 0xFF));
      uint32_t pr5 = Generic.APM1s[5].p(pr0, ctx2);
      uint32_t pr6 = Generic.APM1s[6].p(pr0, ctx3);

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;
      pr = (prA + prB + 1) >> 1;
    }
  }
  return pr;
}
