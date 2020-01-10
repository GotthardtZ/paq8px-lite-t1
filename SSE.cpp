#include "SSE.hpp"

SSE::SSE(ModelStats *st) : stats(st),
        Text {{/*APM:*/  {0x10000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, /* APM: contexts, steps */
              {/*APM1:*/ {0x10000, 7},  {0x10000, 6},  {0x10000, 6}} /* APM1: contexts, rate */
        }, Image {{/*APM:*/ {{0x1000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, /*APM1:*/ {{0x10000, 7}, {0x10000, 7}}}, // color
                  {/*APM:*/ {{0x1000, 24}, {0x10000, 24}, {0x10000, 24}, {0x10000, 24}}, /*APM1:*/ {{0x10000, 5}, {0x10000, 6}}}, // palette
                  {/*APM:*/ {{0x1000, 24}, {0x10000, 24}, {0x10000, 24}}} //gray
        }, Generic {/*APM1:*/ {{0x2000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}, {0x10000, 7}}} {}

int SSE::p(int pr0) {
  INJECT_STATS_blpos
  INJECT_SHARED_c0
  INJECT_SHARED_bpos
  INJECT_SHARED_c4
  int pr;
  int pr1, pr2, pr3;
  switch( stats->blockType ) {
    case TEXT:
    case TEXT_EOL: {
      int limit = 0x3FF >> ((blpos < 0xFFF) * 2);
      pr = Text.APMs[0].p(pr0, (c0 << 8) | (stats->Text.mask & 0xF) | ((stats->misses & 0xF) << 4), limit);
      pr1 = Text.APMs[1].p(pr0, finalize64(hash(bpos, stats->misses & 3, c4 & 0xffff, stats->Text.mask >> 4), 16), limit);
      pr2 = Text.APMs[2].p(pr0, finalize64(hash(c0, stats->Match.expectedByte, stats->Match.length3), 16), limit);
      pr3 = Text.APMs[3].p(pr0, finalize64(hash(c0, c4 & 0xffff, stats->Text.firstLetter), 16), limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2;

      pr1 = Text.APM1s[0].p(pr0, finalize64(hash(stats->Match.expectedByte, stats->Match.length3, c4 & 0xff), 16));
      pr2 = Text.APM1s[1].p(pr, finalize64(hash(c0, c4 & 0x00ffffff), 16));
      pr3 = Text.APM1s[2].p(pr, finalize64(hash(c0, c4 & 0xffffff00), 16));

      pr = (pr + pr1 + pr2 + pr3 + 2) >> 2;
      pr = (pr + pr0 + 1) >> 1;
      break;
    }
    case IMAGE24:
    case IMAGE32: {
      int limit = 0x3FF >> ((blpos < 0xFFF) * 4);
      pr = Image.Color.APMs[0].p(pr0, (c0 << 4) | (stats->misses & 0xF), limit);
      pr1 = Image.Color.APMs[1].p(pr0, finalize64(hash(c0, stats->Image.pixels.W, stats->Image.pixels.WW), 16), limit);
      pr2 = Image.Color.APMs[2].p(pr0, finalize64(hash(c0, stats->Image.pixels.N, stats->Image.pixels.NN), 16), limit);
      pr3 = Image.Color.APMs[3].p(pr0, (c0 << 8) | stats->Image.ctx, limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2;
      pr1 = Image.Color.APM1s[0].p(pr,
                                   finalize64(hash(c0, stats->Image.pixels.W, (c4 & 0xff) - stats->Image.pixels.Wp1, stats->Image.plane),
                                              16));
      pr2 = Image.Color.APM1s[1].p(pr,
                                   finalize64(hash(c0, stats->Image.pixels.N, (c4 & 0xff) - stats->Image.pixels.Np1, stats->Image.plane),
                                              16));

      pr = (pr * 2 + pr1 * 3 + pr2 * 3 + 4) >> 3;
      pr = (pr + pr0 + 1) >> 1;
      break;
    }
    case IMAGE8GRAY: {
      int limit = 0x3FF >> ((blpos < 0xFFF) * 4);
      pr = Image.Gray.APMs[0].p(pr0, (c0 << 4) | (stats->misses & 0xF), limit);
      pr1 = Image.Gray.APMs[1].p(pr, (c0 << 8) | stats->Image.ctx, limit);
      pr2 = Image.Gray.APMs[2].p(pr0, bpos | (stats->Image.ctx & 0xF8) | (stats->Match.expectedByte << 8), limit);

      pr0 = (2 * pr0 + pr1 + pr2 + 2) >> 2;
      pr = (pr + pr0 + 1) >> 1;
      break;
    }
    case IMAGE8: {
      int limit = 0x3FF >> ((blpos < 0xFFF) * 4);
      pr = Image.Palette.APMs[0].p(pr0, (c0 << 4) | (stats->misses & 0xF), limit);
      pr1 = Image.Palette.APMs[1].p(pr0, finalize64(hash(c0 | stats->Image.pixels.W << 8 | stats->Image.pixels.N << 16), 16), limit);
      pr2 = Image.Palette.APMs[2].p(pr0, finalize64(hash(c0 | stats->Image.pixels.N << 8 | stats->Image.pixels.NN << 16), 16), limit);
      pr3 = Image.Palette.APMs[3].p(pr0, finalize64(hash(c0 | stats->Image.pixels.W << 8 | stats->Image.pixels.WW << 16), 16), limit);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2;
      pr1 = Image.Palette.APM1s[0].p(pr0, finalize64(hash(c0 | stats->Match.expectedByte << 8 | stats->Image.pixels.N << 16), 16));
      pr2 = Image.Palette.APM1s[1].p(pr, finalize64(hash(c0 | stats->Image.pixels.W << 8 | stats->Image.pixels.N << 16), 16));

      pr = (pr * 2 + pr1 + pr2 + 2) >> 2;
      pr = (pr + pr0 + 1) >> 1;
      break;
    }
    case JPEG: {
      pr = pr0;
      break;
    }
    default: {
      pr = Generic.APM1s[0].p(pr0, (stats->Match.length3) << 11 | c0 << 3 | (stats->misses & 0x7));
      const uint16_t ctx1 = c0 | (c4 & 0xff) << 8;
      const uint16_t ctx2 = c0 ^finalize64(hash(c4 & 0xffff), 16);
      const uint16_t ctx3 = c0 ^finalize64(hash(c4 & 0xffffff), 16);
      pr1 = Generic.APM1s[1].p(pr0, ctx1);
      pr2 = Generic.APM1s[2].p(pr0, ctx2);
      pr3 = Generic.APM1s[3].p(pr0, ctx3);

      pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2;
      pr1 = Generic.APM1s[4].p(pr, (stats->Match.expectedByte << 8) | (c4 & 0xff));
      pr2 = Generic.APM1s[5].p(pr, ctx2);
      pr3 = Generic.APM1s[6].p(pr, ctx3);

      pr = (pr + pr1 + pr2 + pr3 + 2) >> 2;
      pr = (pr + pr0 + 1) >> 1;
    }
  }
  return pr;
}