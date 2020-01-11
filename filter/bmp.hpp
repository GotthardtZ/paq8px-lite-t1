#ifndef PAQ8PX_BMP_HPP
#define PAQ8PX_BMP_HPP

// 24-bit image data transforms, controlled by OPTION_SKIPRGB:
// - simple color transform (b, g, r) -> (g, g-r, g-b)
// - channel reorder only (b, g, r) -> (g, r, b)
// Detects RGB565 to RGB888 conversions

#define RGB565_MIN_RUN 63

static void encodeBmp(File *in, File *out, uint64_t len, int width) {
  Shared *shared = Shared::getInstance();
  int r, g, b, total = 0;
  bool isPossibleRGB565 = true;
  for( int i = 0; i < (int) (len / width); i++ ) {
    for( int j = 0; j < width / 3; j++ ) {
      b = in->getchar();
      g = in->getchar();
      r = in->getchar();
      if( isPossibleRGB565 ) {
        int pTotal = total;
        total = min(total + 1, 0xFFFF) *
                ((b & 7) == ((b & 8) - ((b >> 3) & 1)) && (g & 3) == ((g & 4) - ((g >> 2) & 1)) && (r & 7) == ((r & 8) - ((r >> 3) & 1)));
        if( total > RGB565_MIN_RUN || pTotal >= RGB565_MIN_RUN ) {
          b ^= (b & 8) - ((b >> 3) & 1);
          g ^= (g & 4) - ((g >> 2) & 1);
          r ^= (r & 8) - ((r >> 3) & 1);
        }
        isPossibleRGB565 = total > 0;
      }
      out->putChar(g);
      out->putChar(shared->options & OPTION_SKIPRGB ? r : g - r);
      out->putChar(shared->options & OPTION_SKIPRGB ? b : g - b);
    }
    for( int j = 0; j < width % 3; j++ )
      out->putChar(in->getchar());
  }
  for( int i = len % width; i > 0; i-- )
    out->putChar(in->getchar());
}

static uint64_t decodeBmp(Encoder &en, uint64_t size, int width, File *out, FMode mode, uint64_t &diffFound) {
  Shared *shared = Shared::getInstance();
  int r, g, b, p, total = 0;
  bool isPossibleRGB565 = true;
  for( int i = 0; i < (int) (size / width); i++ ) {
    p = i * width;
    for( int j = 0; j < width / 3; j++ ) {
      g = en.decompress();
      r = en.decompress();
      b = en.decompress();
      if( !(shared->options & OPTION_SKIPRGB))
        r = g - r, b = g - b;
      if( isPossibleRGB565 ) {
        if( total >= RGB565_MIN_RUN ) {
          b ^= (b & 8) - ((b >> 3) & 1);
          g ^= (g & 4) - ((g >> 2) & 1);
          r ^= (r & 8) - ((r >> 3) & 1);
        }
        total = min(total + 1, 0xFFFF) *
                ((b & 7) == ((b & 8) - ((b >> 3) & 1)) && (g & 3) == ((g & 4) - ((g >> 2) & 1)) && (r & 7) == ((r & 8) - ((r >> 3) & 1)));
        isPossibleRGB565 = total > 0;
      }
      if( mode == FDECOMPRESS ) {
        out->putChar(b);
        out->putChar(g);
        out->putChar(r);
        if( !j && !(i & 0xf))
          en.printStatus();
      } else if( mode == FCOMPARE ) {
        if((b & 255) != out->getchar() && !diffFound )
          diffFound = p + 1;
        if( g != out->getchar() && !diffFound )
          diffFound = p + 2;
        if((r & 255) != out->getchar() && !diffFound )
          diffFound = p + 3;
        p += 3;
      }
    }
    for( int j = 0; j < width % 3; j++ ) {
      if( mode == FDECOMPRESS ) {
        out->putChar(en.decompress());
      } else if( mode == FCOMPARE ) {
        if( en.decompress() != out->getchar() && !diffFound )
          diffFound = p + j + 1;
      }
    }
  }
  for( int i = size % width; i > 0; i-- ) {
    if( mode == FDECOMPRESS ) {
      out->putChar(en.decompress());
    } else if( mode == FCOMPARE ) {
      if( en.decompress() != out->getchar() && !diffFound ) {
        diffFound = size - i;
        break;
      }
    }
  }
  return size;
}

#endif //PAQ8PX_BMP_HPP
