#include "SSE.hpp"

SSE::SSE(ModelStats *st) : stats(st), Text {{{0x10000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, /* APM: contexts, steps */
                                            {{0x10000, 7},  {0x10000, 6},  {0x10000, 6}} /* APM1: contexts, rate */
}, Image {{{{0x1000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, {{0x10000, 7}, {0x10000, 7}}}, // color
          {{{0x1000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, {{0x10000, 5}, {0x10000, 6}}}, // palette
          {{{0x1000, 24}, {0x10000, 24}, {0x10000, 24}}} //gray
}, Generic {{{0x2000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}}} {}

auto SSE::p(int pr0) -> int {
  const uint8_t c0 = shared->c0;
  const uint8_t bitPosition = shared->bitPosition;
  const uint32_t c4 = shared->c4;
  int pr = 0;
  int pr1 = 0;
  int pr2 = 0;
  int pr3 = 0;
  switch( stats->blockType ) {
    case TEXT:
    case TEXT_EOL: {
      int limit = 0x3FFU >> (static_cast<int>(stats->blPos < 0xFFF) * 2);
      pr = Text.APMs[0].p(pr0, (c0 << 8U) | (stats->Text.mask & 0xFU) | ((stats->misses & 0xFU) << 4U), limit);
      pr1 = Text.APMs[1].p(pr0, finalize64(hash(bitPosition, stats->misses & 3U, c4 & 0xffffU, stats->Text.mask >> 4U), 16), limit);
      pr2 = Text.APMs[2].p(pr0, finalize64(hash(c0, stats->Match.expectedByte, stats->Match.length3), 16), limit);
      pr3 = Text.APMs[3].p(pr0, finalize64(hash(c0, c4 & 0xffffU, stats->Text.firstLetter), 16), limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2U;

      pr1 = Text.APM1s[0].p(pr0, finalize64(hash(stats->Match.expectedByte, stats->Match.length3, c4 & 0xffU), 16));
      pr2 = Text.APM1s[1].p(pr, finalize64(hash(c0, c4 & 0x00ffffffU), 16));
      pr3 = Text.APM1s[2].p(pr, finalize64(hash(c0, c4 & 0xffffff00U), 16));

      pr = (pr + pr1 + pr2 + pr3 + 2) >> 2U;
      pr = (pr + pr0 + 1) >> 1U;
      break;
    }
    case IMAGE24:
    case IMAGE32: {
      int limit = 0x3FFU >> (static_cast<int>(stats->blPos < 0xFFFU) * 4);
      pr = Image.Color.APMs[0].p(pr0, (c0 << 4U) | (stats->misses & 0xFU), limit);
      pr1 = Image.Color.APMs[1].p(pr0, finalize64(hash(c0, stats->Image.pixels.W, stats->Image.pixels.WW), 16), limit);
      pr2 = Image.Color.APMs[2].p(pr0, finalize64(hash(c0, stats->Image.pixels.N, stats->Image.pixels.NN), 16), limit);
      pr3 = Image.Color.APMs[3].p(pr0, (c0 << 8U) | stats->Image.ctx, limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2U;
      pr1 = Image.Color.APM1s[0].p(pr,
                                   finalize64(hash(c0, stats->Image.pixels.W, (c4 & 0xffU) - stats->Image.pixels.Wp1, stats->Image.plane),
                                              16));
      pr2 = Image.Color.APM1s[1].p(pr,
                                   finalize64(hash(c0, stats->Image.pixels.N, (c4 & 0xffU) - stats->Image.pixels.Np1, stats->Image.plane),
                                              16));

      pr = (pr * 2 + pr1 * 3 + pr2 * 3 + 4) >> 3U;
      pr = (pr + pr0 + 1) >> 1U;
      break;
    }
    case IMAGE8GRAY: {
      int limit = 0x3FFU >> (static_cast<int>(stats->blPos < 0xFFF) * 4);
      pr = Image.Gray.APMs[0].p(pr0, (c0 << 4) | (stats->misses & 0xFU), limit);
      pr1 = Image.Gray.APMs[1].p(pr, (c0 << 8) | stats->Image.ctx, limit);
      pr2 = Image.Gray.APMs[2].p(pr0, bitPosition | (stats->Image.ctx & 0xF8U) | (stats->Match.expectedByte << 8U), limit);

      pr0 = (2 * pr0 + pr1 + pr2 + 2) >> 2U;
      pr = (pr + pr0 + 1) >> 1U;
      break;
    }
    case IMAGE8: {
      int limit = 0x3FFU >> (static_cast<int>(stats->blPos < 0xFFFU) * 4);
      pr = Image.Palette.APMs[0].p(pr0, (c0 << 4U) | (stats->misses & 0xFU), limit);
      pr1 = Image.Palette.APMs[1].p(pr0, finalize64(hash(c0 | stats->Image.pixels.W << 8U | stats->Image.pixels.N << 16U), 16), limit);
      pr2 = Image.Palette.APMs[2].p(pr0, finalize64(hash(c0 | stats->Image.pixels.N << 8U | stats->Image.pixels.NN << 16U), 16), limit);
      pr3 = Image.Palette.APMs[3].p(pr0, finalize64(hash(c0 | stats->Image.pixels.W << 8U | stats->Image.pixels.WW << 16U), 16), limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2U;
      pr1 = Image.Palette.APM1s[0].p(pr0, finalize64(hash(c0 | stats->Match.expectedByte << 8U | stats->Image.pixels.N << 16U), 16));
      pr2 = Image.Palette.APM1s[1].p(pr, finalize64(hash(c0 | stats->Image.pixels.W << 8U | stats->Image.pixels.N << 16U), 16));

      pr = (pr * 2 + pr1 + pr2 + 2) >> 2U;
      pr = (pr + pr0 + 1) >> 1U;
      break;
    }
    case JPEG: {
      pr = pr0;
      break;
    }
    default: {
      pr = Generic.APM1s[0].p(pr0, (stats->Match.length3) << 11U | c0 << 3U | (stats->misses & 0x7U));
      const uint16_t ctx1 = c0 | (c4 & 0xffU) << 8U;
      const uint16_t ctx2 = c0 ^finalize64(hash(c4 & 0xffffU), 16);
      const uint16_t ctx3 = c0 ^finalize64(hash(c4 & 0xffffffU), 16);
      pr1 = Generic.APM1s[1].p(pr0, ctx1);
      pr2 = Generic.APM1s[2].p(pr0, ctx2);
      pr3 = Generic.APM1s[3].p(pr0, ctx3);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2U;
      pr1 = Generic.APM1s[4].p(pr, (stats->Match.expectedByte << 8U) | (c4 & 0xffu));
      pr2 = Generic.APM1s[5].p(pr, ctx2);
      pr3 = Generic.APM1s[6].p(pr, ctx3);

      pr = (pr + pr1 + pr2 + pr3 + 2) >> 2U;
      pr = (pr + pr0 + 1) >> 1U;
    }
  }
  return pr;
}