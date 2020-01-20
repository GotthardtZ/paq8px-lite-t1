#include "JpegModel.hpp"

JpegModel::JpegModel(const uint64_t size) : t(size), MJPEGMap(21, 3, 128, 127), /* BitsOfContext, InputBits, Scale, Limit */
        sm(N, 256, 1023, StateMap::BitHistory), apm1(0x8000, 24), apm2(0x20000, 24) {
  auto mf = new MixerFactory();
  m1 = mf->createMixer(N + 1, 2050, 3);
  m1->setScaleFactor(1024, 128);
}

JpegModel::~JpegModel() {
  delete m1;
}

auto JpegModel::mix(Mixer &m) -> int {
  static constexpr uint8_t zzu[64] = {// zigzag coefficient -> u,v
          0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7,
          7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 5, 6, 7, 7, 6, 7};
  static constexpr uint8_t zzv[64] = {0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5,
                                      6, 7, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 6, 7, 7};

  // Standard Huffman tables (cf. JPEG standard section K.3)
  // IMPORTANT: these are only valid for 8-bit data precision
  static constexpr uint8_t bitsDcLuminance[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
  static constexpr uint8_t valuesDcLuminance[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  static constexpr uint8_t bitsDcChrominance[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
  static constexpr uint8_t valuesDcChrominance[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  static constexpr uint8_t bitsAcLuminance[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
  static constexpr uint8_t valuesAcLuminance[162] = {0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51,
                                                     0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1,
                                                     0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18,
                                                     0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                                                     0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57,
                                                     0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
                                                     0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92,
                                                     0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                                     0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
                                                     0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
                                                     0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2,
                                                     0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};

  static constexpr uint8_t bitsAcChrominance[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
  static constexpr uint8_t valuesAcChrominance[162] = {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07,
                                                       0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09,
                                                       0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25,
                                                       0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
                                                       0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
                                                       0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
                                                       0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                                                       0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
                                                       0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
                                                       0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
                                                       0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2,
                                                       0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};

  if( idx < 0 ) {
    memset(&images[0], 0, sizeof(images));
    idx = 0;
    lastPos = shared->buf.getpos();
  }

  INJECT_SHARED_buf

  // Be sure to quit on a byte boundary
  if( shared->bitPosition == 0 ) {
    images[idx].nextJpeg = static_cast<uint32_t>(images[idx].jpeg > 1);
  }
  if( shared->bitPosition != 0 && (images[idx].jpeg == 0u)) {
    return images[idx].nextJpeg;
  }
  if( shared->bitPosition == 0 && images[idx].app > 0 ) {
    --images[idx].app;
    if( idx < maxEmbeddedLevel && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
        ((buf(1) & 0xFEU) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE))) {
      memset(&images[++idx], 0, sizeof(JPEGImage));
    }
  }
  if( images[idx].app > 0 ) {
    return images[idx].nextJpeg;
  }
  if( shared->bitPosition == 0 ) {
    // Parse.  Baseline DCT-Huffman JPEG syntax is:
    // SOI APPx... misc... SOF0 DHT... SOS data EOI
    // SOI (= FF D8) start of image.
    // APPx (= FF Ex) len ... where len is always a 2 byte big-endian length
    //   including the length itself but not the 2 byte preceding code.
    //   Application data is ignored.  There may be more than one APPx.
    // misc codes are DQT, DNL, DRI, COM (ignored).
    // SOF0 (= FF C0) len 08 height width Nf [c HV Tq]...
    //   where len, height, width (in pixels) are 2 bytes, Nf is the repeat
    //   count (1 byte) of [c HV Tq], where c is a component identifier
    //   (color, 0-3), HV is the horizontal and vertical dimensions
    //   of the MCU (high, low bits, packed), and Tq is the quantization
    //   table ID (not used).  An MCU (minimum compression unit) consists
    //   of 64*H*V DCT coefficients for each color.
    // DHT (= FF C4) len [TcTh L1...L16 V1,1..V1,L1 ... V16,1..V16,L16]...
    //   defines Huffman table Th (1-4) for Tc (0=DC (first coefficient)
    //   1=AC (next 63 coefficients)).  L1..L16 are the number of codes
    //   of length 1-16 (in ascending order) and Vx,y are the 8-bit values.
    //   A V code of RS means a run of R (0-15) zeros followed by s (0-15)
    //   additional bits to specify the next nonzero value, negative if
    //   the first additional bit is 0 (e.g. code x63 followed by the
    //   3 bits 1,0,1 specify 7 coefficients: 0, 0, 0, 0, 0, 0, 5.
    //   code 00 means end of block (remainder of 63 AC coefficients is 0).
    // SOS (= FF DA) len Ns [Cs TdTa]... 0 3F 00
    //   start of scan.  TdTa specifies DC/AC Huffman tables (0-3, packed
    //   into one byte) for component Cs matching c in SOF0, repeated
    //   Ns (1-4) times.
    // EOI (= FF D9) is end of image.
    // Huffman coded data is between SOI and EOI.  Codes may be embedded:
    // RST0-RST7 (= FF D0 to FF D7) mark the start of an independently
    //   compressed region.
    // DNL (= FF DC) 04 00 height
    //   might appear at the end of the scan (ignored).
    // FF 00 is interpreted as FF (to distinguish from RSTx, DNL, EOI).

    // Detect JPEG (SOI followed by a valid marker)
    if((images[idx].jpeg == 0u) && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
       ((buf(1) & 0xFEU) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE))) {
      images[idx].jpeg = 1;
      images[idx].offset = shared->buf.getpos() - 4;
      images[idx].sos = images[idx].sof = images[idx].htSize = images[idx].data = 0, images[idx].app =
              static_cast<int>(buf(1) >> 4 == 0xE) * 2;
      mcusize = huffcode = huffBits = huffSize = mcuPos = cPos = 0, rs = -1;
      memset(&huf[0], 0, sizeof(huf));
      memset(&pred[0], 0, pred.size() * sizeof(int));
      resetPos = resetLen = 0;
    }

    // Detect end of JPEG when data contains a marker other than RSTx
    // or byte stuff (00), or if we jumped in position since the last byte seen
    if((images[idx].jpeg != 0u) && (images[idx].data != 0u) &&
       ((buf(2) == FF && (buf(1) != 0u) && (buf(1) & 0xf8U) != RST0) || (shared->buf.getpos() - lastPos > 1))) {
      JASSERT((buf(1) == EOI) || (shared->buf.getpos() - lastPos > 1))
      FINISH(true)
    }
    lastPos = shared->buf.getpos();
    if( images[idx].jpeg == 0u ) {
      return images[idx].nextJpeg;
    }

    // Detect APPx, COM or other markers, so we can skip them
    if((images[idx].data == 0u) && (images[idx].app == 0u) && buf(4) == FF &&
       (((buf(3) > 0xC1) && (buf(3) <= 0xCF) && (buf(3) != DHT)) || ((buf(3) >= 0xDC) && (buf(3) <= 0xFE)))) {
      images[idx].app = buf(2) * 256 + buf(1) + 2;
      if( idx > 0 ) {
        JASSERT(shared->buf.getpos() + images[idx].app < images[idx].offset + images[idx - 1].app)

      }
    }

    // Save pointers to sof, ht, sos, data,
    if( buf(5) == FF && buf(4) == SOS ) {
      int len = buf(3) * 256 + buf(2);
      if( len == 6 + 2 * buf(1) && (buf(1) != 0u) && buf(1) <= 4 ) { // buf(1) is Ns
        images[idx].sos = shared->buf.getpos() - 5, images[idx].data = images[idx].sos + len + 2, images[idx].jpeg = 2;
      }
    }
    if( buf(4) == FF && buf(3) == DHT && images[idx].htSize < 8 ) {
      images[idx].ht[images[idx].htSize++] = shared->buf.getpos() - 4;
    }
    if( buf(4) == FF && (buf(3) & 0xFE) == SOF0 ) {
      images[idx].sof = shared->buf.getpos() - 4;
    }

    // Parse quantization tables
    if( buf(4) == FF && buf(3) == DQT ) {
      dqtEnd = shared->buf.getpos() + buf(2) * 256 + buf(1) - 1, dqt_state = 0;
    } else if( dqt_state >= 0 ) {
      if( shared->buf.getpos() >= dqtEnd ) {
        dqt_state = -1;
      } else {
        if( dqt_state % 65 == 0 ) {
          qNum = buf(1);
        } else {
          JASSERT(buf(1) > 0)
          JASSERT(qNum >= 0 && qNum < 4)
          images[idx].qTable[qNum * 64 + ((dqt_state % 65) - 1)] = buf(1) - 1;
        }
        dqt_state++;
      }
    }

    // Restart
    if( buf(2) == FF && (buf(1) & 0xf8U) == RST0 ) {
      huffcode = huffBits = huffSize = mcuPos = 0, rs = -1;
      memset(&pred[0], 0, pred.size() * sizeof(int));
      resetLen = column + row * width - resetPos;
      resetPos = column + row * width;
    }
  }

  {
    // Build Huffman tables
    // huf[Tc][Th][m] = min, max+1 codes of length m, pointer to byte values
    if( shared->buf.getpos() == images[idx].data && shared->bitPosition == 1 ) {
      for( uint32_t i = 0; i < images[idx].htSize; ++i ) {
        uint32_t p = images[idx].ht[i] + 4; // pointer to current table after length field
        uint32_t end = p + buf[p - 2] * 256 + buf[p - 1] - 2; // end of Huffman table
        uint32_t count = 0; // sanity check
        while( p < end && end < shared->buf.getpos() && end < p + 2100 && ++count < 10 ) {
          int tc = buf[p] >> 4U;
          int th = buf[p] & 15U;
          if( tc >= 2 || th >= 4 ) {
            break;
          }
          JASSERT(tc >= 0 && tc < 2 && th >= 0 && th < 4)
          HUF *h = &huf[tc * 64 + th * 16]; // [tc][th][0];
          int val = p + 17; // pointer to values
          int hval = tc * 1024 + th * 256; // pointer to RS values in hBuf
          int j = 0;
          for( j = 0; j < 256; ++j ) { // copy RS codes
            hBuf[hval + j] = buf[val + j];
          }
          int code = 0;
          for( j = 0; j < 16; ++j ) {
            h[j].min = code;
            h[j].max = code += buf[p + j + 1];
            h[j].val = hval;
            val += buf[p + j + 1];
            hval += buf[p + j + 1];
            code *= 2;
          }
          p = val;
          JASSERT(hval >= 0 && hval < 2048)
        }
        JASSERT(p == end)
      }
      huffcode = huffBits = huffSize = 0, rs = -1;

      // load default tables
      if( images[idx].htSize == 0u ) {
        for( int tc = 0; tc < 2; tc++ ) {
          for( int th = 0; th < 2; th++ ) {
            HUF *h = &huf[tc * 64 + th * 16];
            int hval = tc * 1024 + th * 256;
            int code = 0;
            int c = 0;
            int x = 0;

            for( int i = 0; i < 16; i++ ) {
              switch( tc * 2 + th ) {
                case 0:
                  x = bitsDcLuminance[i];
                  break;
                case 1:
                  x = bitsDcChrominance[i];
                  break;
                case 2:
                  x = bitsAcLuminance[i];
                  break;
                case 3:
                  x = bitsAcChrominance[i];
              }

              h[i].min = code;
              h[i].max = (code += x);
              h[i].val = hval;
              hval += x;
              code += code;
              c += x;
            }

            hval = tc * 1024 + th * 256;
            c--;

            while( c >= 0 ) {
              switch( tc * 2 + th ) {
                case 0:
                  x = valuesDcLuminance[c];
                  break;
                case 1:
                  x = valuesDcChrominance[c];
                  break;
                case 2:
                  x = valuesAcLuminance[c];
                  break;
                case 3:
                  x = valuesAcChrominance[c];
              }

              hBuf[hval + c] = x;
              c--;
            }
          }
        }
        images[idx].htSize = 4;
      }

      // Build Huffman table selection table (indexed by mcuPos).
      // Get image width.
      if((images[idx].sof == 0u) && (images[idx].sos != 0u)) {
        return images[idx].nextJpeg;
      }
      int ns = buf[images[idx].sos + 4];
      int nf = buf[images[idx].sof + 9];
      JASSERT(ns <= 4 && nf <= 4)
      mcusize = 0; // blocks per MCU
      int hmax = 0; // MCU horizontal dimension
      for( int i = 0; i < ns; ++i ) {
        for( int j = 0; j < nf; ++j ) {
          if( buf[images[idx].sos + 2 * i + 5] == buf[images[idx].sof + 3 * j + 10] ) { // Cs == c ?
            int hv = buf[images[idx].sof + 3 * j + 11]; // packed dimensions H x V
            samplingFactors[j] = hv;
            if( hv >> 4U > hmax ) {
              hmax = hv >> 4U;
            }
            hv = (hv & 15U) * (hv >> 4U); // number of blocks in component c
            JASSERT(hv >= 1 && hv + mcusize <= 10)
            while( hv != 0 ) {
              JASSERT(mcusize < 10)
              hufSel[0][mcusize] = buf[images[idx].sos + 2 * i + 6] >> 4 & 15;
              hufSel[1][mcusize] = buf[images[idx].sos + 2 * i + 6] & 15;
              JASSERT(hufSel[0][mcusize] < 4 && hufSel[1][mcusize] < 4)
              color[mcusize] = i;
              int tq = buf[images[idx].sof + 3 * j + 12]; // quantization table index (0..3)
              JASSERT(tq >= 0 && tq < 4)
              images[idx].qMap[mcusize] = tq; // quantization table mapping
              --hv;
              ++mcusize;
            }
          }
        }
      }
      JASSERT(hmax >= 1 && hmax <= 10)
      int j = 0;
      for( j = 0; j < mcusize; ++j ) {
        ls[j] = 0;
        for( int i = 1; i < mcusize; ++i ) {
          if( color[(j + i) % mcusize] == color[j] ) {
            ls[j] = i;
          }
        }
        ls[j] = (mcusize - ls[j]) << 6U;
      }
      for( j = 0; j < 64; ++j ) {
        zPos[zzu[j] + 8 * zzv[j]] = j;
      }
      width = buf[images[idx].sof + 7] * 256 + buf[images[idx].sof + 8]; // in pixels
      width = (width - 1) / (hmax * 8) + 1; // in MCU
      JASSERT(width > 0)
      mcusize *= 64; // coefficients per MCU
      row = column = 0;

      // we can have more blocks than components then we have subsampling
      int x = 0;
      int y = 0;
      for( j = 0; j < (mcusize >> 6U); j++ ) {
        int i = color[j];
        int w = samplingFactors[i] >> 4U;
        int h = samplingFactors[i] & 0xfU;
        blockW[j] = x == 0 ? mcusize - 64 * (w - 1) : 64;
        blockN[j] = y == 0 ? mcusize * width - 64 * w * (h - 1) : w * 64;
        x++;
        if( x >= w ) {
          x = 0;
          y++;
        }
        if( y >= h ) {
          x = 0;
          y = 0;
        }
      }
    }
  }


  // Decode Huffman
  {
    if((mcusize != 0) && buf(1 + static_cast<int>(shared->bitPosition == 0)) != FF ) { // skip stuffed byte
      JASSERT(huffBits <= 32)
      INJECT_SHARED_y
      huffcode += huffcode + y;
      ++huffBits;
      if( rs < 0 ) {
        JASSERT(huffBits >= 1 && huffBits <= 16)
        const int ac = static_cast<const int>((mcuPos & 63U) > 0);
        JASSERT(mcuPos >= 0 && (mcuPos >> 6U) < 10)
        JASSERT(ac == 0 || ac == 1)
        const int sel = hufSel[ac][mcuPos >> 6U];
        JASSERT(sel >= 0 && sel < 4)
        const int i = huffBits - 1;
        JASSERT(i >= 0 && i < 16)
        const HUF *h = &huf[ac * 64 + sel * 16]; // [ac][sel];
        JASSERT(h[i].min <= h[i].max && h[i].val < 2048 && huffBits > 0)
        if( huffcode < h[i].max ) {
          JASSERT(huffcode >= h[i].min)
          int k = h[i].val + huffcode - h[i].min;
          JASSERT(k >= 0 && k < 2048)
          rs = hBuf[k];
          huffSize = huffBits;
        }
      }
      if( rs >= 0 ) {
        if( huffSize + (rs & 15) == huffBits ) { // done decoding
          rs1 = rs;
          int ex = 0; // decoded extra bits
          if((mcuPos & 63U) != 0u ) { // AC
            if( rs == 0 ) { // EOB
              mcuPos = (mcuPos + 63) & -64U;
              JASSERT(mcuPos >= 0 && mcuPos <= mcusize && mcuPos <= 640)
              while((cPos & 63U) != 0u ) {
                cBuf2.set(cPos, 0);
                coefficientBuffer.set(cPos, (rs == 0) ? 0 : (63 - (cPos & 63U)) << 4U);
                cPos++;
                rs++;
              }
            } else { // rs = r zeros + s extra bits for the next nonzero value
              // If first extra bit is 0 then value is negative.
              JASSERT((rs & 15U) <= 10)
              const int r = rs >> 4U;
              const int s = rs & 15U;
              JASSERT(mcuPos >> 6U == (mcuPos + r) >> 6)
              mcuPos += r + 1;
              ex = huffcode & ((1U << s) - 1);
              if( s != 0 && (ex >> (s - 1)) == 0 ) {
                ex -= (1U << s) - 1;
              }
              for( int i = r; i >= 1; --i ) {
                cBuf2.set(cPos, 0);
                coefficientBuffer.set(cPos, (i << 4U) | s);
                cPos++;
              }
              cBuf2.set(cPos, ex);
              coefficientBuffer.set(cPos, (s << 4U) | (huffcode << 2U >> s & 3U) | 12);
              cPos++;
              sSum += s;
            }
          } else { // DC: rs = 0S, s<12
            JASSERT(rs < 12)
            ++mcuPos;
            ex = huffcode & ((1U << rs) - 1);
            if( rs != 0 && (ex >> (rs - 1)) == 0 ) {
              ex -= (1U << rs) - 1;
            }
            JASSERT(mcuPos >= 0 && mcuPos >> 6U < 10)
            const int comp = color[mcuPos >> 6U];
            JASSERT(comp >= 0 && comp < 4)
            dc = pred[comp] += ex;
            JASSERT((cPos & 63U) == 0)
            cBuf2.set(cPos, dc);
            coefficientBuffer.set(cPos, (dc + 1023) >> 3U);
            cPos++;
            if((mcuPos >> 6U) == 0 ) {
              sSum1 = 0;
              sSum2 = sSum3;
            } else {
              if( color[(mcuPos >> 6U) - 1] == color[0] ) {
                sSum1 += (sSum3 = sSum);
              }
              sSum2 = sSum1;
            }
            sSum = rs;
          }
          JASSERT(mcuPos >= 0 && mcuPos <= mcusize)
          if( mcuPos >= mcusize ) {
            mcuPos = 0;
            if( ++column == width ) {
              column = 0, ++row;
            }
          }
          huffcode = huffSize = huffBits = 0, rs = -1;

          // UPDATE_ADV_PRED !!!!
          {
            const int aComp = mcuPos >> 6U;
            const int q = 64 * images[idx].qMap[aComp];
            const int zz = mcuPos & 63U;
            const int cposDc = cPos - zz;
            const bool noReset = resetPos != column + row * width;
            if( zz == 0 ) {
              for( int i = 0; i < 8; ++i ) {
                sumU[i] = sumV[i] = 0;
              }
              // position in the buffer of first (DC) coefficient of the block
              // of this same component that is to the west of this one, not
              // necessarily in this MCU
              int offsetDcW = cposDc - blockW[aComp];
              // position in the buffer of first (DC) coefficient of the block
              // of this same component that is to the north of this one, not
              // necessarily in this MCU
              int offsetDcN = cposDc - blockN[aComp];
              for( int i = 0; i < 64; ++i ) {
                sumU[zzu[i]] += ((zzv[i] & 1) != 0 ? -1 : 1) * (zzv[i] != 0u ? 16 * (16 + zzv[i]) : 185) * (images[idx].qTable[q + i] + 1) *
                                cBuf2[offsetDcN + i];
                sumV[zzv[i]] += ((zzu[i] & 1) != 0 ? -1 : 1) * (zzu[i] != 0u ? 16 * (16 + zzu[i]) : 185) * (images[idx].qTable[q + i] + 1) *
                                cBuf2[offsetDcW + i];
              }
            } else {
              sumU[zzu[zz - 1]] -=
                      (zzv[zz - 1] != 0u ? 16 * (16 + zzv[zz - 1]) : 185) * (images[idx].qTable[q + zz - 1] + 1) * cBuf2[cPos - 1];
              sumV[zzv[zz - 1]] -=
                      (zzu[zz - 1] != 0u ? 16 * (16 + zzu[zz - 1]) : 185) * (images[idx].qTable[q + zz - 1] + 1) * cBuf2[cPos - 1];
            }

            for( int i = 0; i < 3; ++i ) {
              runPred[i] = runPred[i + 3] = 0;
              for( int st = 0; st < 10 && zz + st < 64; ++st ) {
                const int zz2 = zz + st;
                int p = sumU[zzu[zz2]] * i + sumV[zzv[zz2]] * (2 - i);
                p /= (images[idx].qTable[q + zz2] + 1) * 185 * (16 + zzv[zz2]) * (16 + zzu[zz2]) / 128;
                if( zz2 == 0 && (noReset || ls[aComp] == 64)) {
                  p -= cBuf2[cposDc - ls[aComp]];
                }
                p = (p < 0 ? -1 : +1) * ilog->log(abs(p) + 1);
                if( st == 0 ) {
                  advPred[i] = p;
                } else if( abs(p) > abs(advPred[i]) + 2 && abs(advPred[i]) < 210 ) {
                  if( runPred[i] == 0 ) {
                    runPred[i] = st * 2 + static_cast<int>(p > 0);
                  }
                  if( abs(p) > abs(advPred[i]) + 21 && runPred[i + 3] == 0 ) {
                    runPred[i + 3] = st * 2 + static_cast<int>(p > 0);
                  }
                }
              }
            }
            ex = 0;
            for( int i = 0; i < 8; ++i ) {
              ex += static_cast<int>(zzu[zz] < i) * sumU[i] + static_cast<int>(zzv[zz] < i) * sumV[i];
            }
            ex = (sumU[zzu[zz]] * (2 + zzu[zz]) + sumV[zzv[zz]] * (2 + zzv[zz]) - ex * 2) * 4 / (zzu[zz] + zzv[zz] + 16);
            ex /= (images[idx].qTable[q + zz] + 1) * 185;
            if( zz == 0 && (noReset || ls[aComp] == 64)) {
              ex -= cBuf2[cposDc - ls[aComp]];
            }
            advPred[3] = (ex < 0 ? -1 : +1) * ilog->log(abs(ex) + 1);

            for( int i = 0; i < 4; ++i ) {
              const int a = (i & 1 ? zzv[zz] : zzu[zz]), b = (i & 2U ? 2 : 1);
              if( a < b )
                ex = 65535;
              else {
                const int zz2 = zPos[zzu[zz] + 8 * zzv[zz] - (i & 1U ? 8 : 1) * b];
                ex = (images[idx].qTable[q + zz2] + 1) * cBuf2[cposDc + zz2] / (images[idx].qTable[q + zz] + 1);
                ex = (ex < 0 ? -1 : +1) * (ilog->log(abs(ex) + 1) + (ex != 0 ? 17 : 0));
              }
              lcp[i] = ex;
            }
            if((zzu[zz] * zzv[zz]) != 0 ) {
              const int zz2 = zPos[zzu[zz] + 8 * zzv[zz] - 9];
              ex = (images[idx].qTable[q + zz2] + 1) * cBuf2[cposDc + zz2] / (images[idx].qTable[q + zz] + 1);
              lcp[4] = (ex < 0 ? -1 : +1) * (ilog->log(abs(ex) + 1) + (ex != 0 ? 17 : 0));

              ex = (images[idx].qTable[q + zPos[8 * zzv[zz]]] + 1) * cBuf2[cposDc + zPos[8 * zzv[zz]]] / (images[idx].qTable[q + zz] + 1);
              lcp[5] = (ex < 0 ? -1 : +1) * (ilog->log(abs(ex) + 1) + (ex != 0 ? 17 : 0));

              ex = (images[idx].qTable[q + zPos[zzu[zz]]] + 1) * cBuf2[cposDc + zPos[zzu[zz]]] / (images[idx].qTable[q + zz] + 1);
              lcp[6] = (ex < 0 ? -1 : +1) * (ilog->log(abs(ex) + 1) + (ex != 0 ? 17 : 0));
            } else {
              lcp[4] = lcp[5] = lcp[6] = 65535;
            }

            int prev1 = 0;
            int prev2 = 0;
            int cnt1 = 0;
            int cnt2 = 0;
            int r = 0;
            int s = 0;
            prevCoefRs = coefficientBuffer[cPos - 64];
            for( int i = 0; i < aComp; i++ ) {
              ex = 0;
              ex += cBuf2[cPos - (aComp - i) * 64];
              if( zz == 0 && (noReset || ls[i] == 64)) {
                ex -= cBuf2[cposDc - (aComp - i) * 64 - ls[i]];
              }
              if( color[i] == color[aComp] - 1 ) {
                prev1 += ex;
                cnt1++;
                r += coefficientBuffer[cPos - (aComp - i) * 64] >> 4U;
                s += coefficientBuffer[cPos - (aComp - i) * 64] & 0xFU;
              }
              if( color[aComp] > 1 && color[i] == color[0] ) {
                prev2 += ex;
                cnt2++;
              }
            }
            if( cnt1 > 0 ) {
              prev1 /= cnt1, r /= cnt1, s /= cnt1, prevCoefRs = (r << 4U) | s;
            }
            if( cnt2 > 0 ) {
              prev2 /= cnt2;
            }
            prevCoef = (prev1 < 0 ? -1 : +1) * ilog->log(11 * abs(prev1) + 1) + (cnt1 << 20U);
            prevCoef2 = (prev2 < 0 ? -1 : +1) * ilog->log(11 * abs(prev2) + 1);

            if( column == 0 && blockW[aComp] > 64 * aComp ) {
              runPred[1] = runPred[2], runPred[0] = 0, advPred[1] = advPred[2], advPred[0] = 0;
            }
            if( row == 0 && blockN[aComp] > 64 * aComp ) {
              runPred[1] = runPred[0], runPred[2] = 0, advPred[1] = advPred[0], advPred[2] = 0;
            }
          } // !!!!
        }
      }
    }
  }

  // Estimate next bit probability
  if((images[idx].jpeg == 0u) || (images[idx].data == 0u)) {
    return images[idx].nextJpeg;
  }
  if( buf(1 + static_cast<int>(shared->bitPosition == 0)) == FF ) {
    m.add(128); //network bias
    m.set(0, 1 + 8);
    m.set(0, 1 + 1024);
    m.set(buf(1), 1024);
    return 1;
  }
  if( resetLen > 0 && resetLen == column + row * width - resetPos && mcuPos == 0 && static_cast<int>(huffcode) == (1U << huffBits) - 1 ) {
    m.add(2047); //network bias
    m.set(0, 1 + 8);
    m.set(0, 1 + 1024);
    m.set(buf(1), 1024);
    return 1;
  }

  // update model
  if( cp[N - 1] != nullptr ) {
    INJECT_SHARED_y
    for( int i = 0; i < N; ++i ) {
      StateTable::update(cp[i], y, rnd);
    }
  }

  // update context
  const int comp = color[mcuPos >> 6U];
  const int coef = (mcuPos & 63U) | comp << 6U;
  const int hc = (huffcode * 4 + static_cast<int>((mcuPos & 63U) == 0) * 2 + static_cast<unsigned int>(comp == 0)) | 1U << (huffBits + 2);
  const bool firstCol = column == 0 && blockW[mcuPos >> 6U] > mcuPos;
  if( ++hbCount > 2 || huffBits == 0 ) {
    hbCount = 0;
  }
  JASSERT(coef >= 0 && coef < 256)
  const int zu = zzu[mcuPos & 63U];
  const int zv = zzv[mcuPos & 63U];
  if( hbCount == 0 ) {
    uint64_t n = hc * 32;
    int i = 0;
    cxt[i++] = hash(++n, coef, advPred[2] / 12 + (runPred[2] << 8U), sSum2 >> 6U, prevCoef / 72);
    cxt[i++] = hash(++n, coef, advPred[0] / 12 + (runPred[0] << 8U), sSum2 >> 6U, prevCoef / 72);
    cxt[i++] = hash(++n, coef, advPred[1] / 11 + (runPred[1] << 8U), sSum2 >> 6U);
    cxt[i++] = hash(++n, rs1, advPred[2] / 7, runPred[5] / 2, prevCoef / 10);
    cxt[i++] = hash(++n, rs1, advPred[0] / 7, runPred[3] / 2, prevCoef / 10);
    cxt[i++] = hash(++n, rs1, advPred[1] / 11, runPred[4]);
    cxt[i++] = hash(++n, advPred[2] / 14, runPred[2], advPred[0] / 14, runPred[0]);
    cxt[i++] = hash(++n, coefficientBuffer[cPos - blockN[mcuPos >> 6U]] >> 4U, advPred[3] / 17, runPred[1], runPred[5]);
    cxt[i++] = hash(++n, coefficientBuffer[cPos - blockW[mcuPos >> 6U]] >> 4U, advPred[3] / 17, runPred[1], runPred[3]);
    cxt[i++] = hash(++n, lcp[0] / 22, lcp[1] / 22, advPred[1] / 7, runPred[1]);
    cxt[i++] = hash(++n, lcp[0] / 22, lcp[1] / 22, mcuPos & 63U, lcp[4] / 30);
    cxt[i++] = hash(++n, zu / 2, lcp[0] / 13, lcp[2] / 30, prevCoef / 40 + ((prevCoef2 / 28) << 20U));
    cxt[i++] = hash(++n, zv / 2, lcp[1] / 13, lcp[3] / 30, prevCoef / 40 + ((prevCoef2 / 28) << 20U));
    cxt[i++] = hash(++n, rs1, prevCoef / 42, prevCoef2 / 34, lcp[0] / 60, lcp[2] / 14, lcp[1] / 60, lcp[3] / 14);
    cxt[i++] = hash(++n, mcuPos & 63U, column >> 1U);
    cxt[i++] = hash(++n, column >> 3U, min(5 + 2 * static_cast<int>(comp == 0), zu + zv), lcp[0] / 10, lcp[2] / 40, lcp[1] / 10,
                    lcp[3] / 40);
    cxt[i++] = hash(++n, sSum >> 3U, mcuPos & 63U);
    cxt[i++] = hash(++n, rs1, mcuPos & 63U, runPred[1]);
    cxt[i++] = hash(++n, coef, sSum2 >> 5U, advPred[3] / 30,
                    (comp) != 0 ? hash(prevCoef / 22, prevCoef2 / 50) : sSum / ((mcuPos & 0x3F) + 1));
    cxt[i++] = hash(++n, lcp[0] / 40, lcp[1] / 40, advPred[1] / 28, (comp) != 0 ? prevCoef / 40 + ((prevCoef2 / 40) << 20U) : lcp[4] / 22,
                    min(7, zu + zv), sSum / (2 * (zu + zv) + 1));
    cxt[i++] = hash(++n, zv, coefficientBuffer[cPos - blockN[mcuPos >> 6U]], advPred[2] / 28, runPred[2]);
    cxt[i++] = hash(++n, zu, coefficientBuffer[cPos - blockW[mcuPos >> 6U]], advPred[0] / 28, runPred[0]);
    cxt[i++] = hash(++n, advPred[2] / 7, runPred[2]);
    cxt[i++] = hash(n, advPred[0] / 7, runPred[0]);
    cxt[i++] = hash(n, advPred[1] / 7, runPred[1]);
    cxt[i++] = hash(++n, zv, lcp[1] / 14, advPred[2] / 16, runPred[5]);
    cxt[i++] = hash(++n, zu, lcp[0] / 14, advPred[0] / 16, runPred[3]);
    cxt[i++] = hash(++n, lcp[0] / 14, lcp[1] / 14, advPred[3] / 16);
    cxt[i++] = hash(++n, coef, prevCoef / 10, prevCoef2 / 20);
    cxt[i++] = hash(++n, coef, sSum >> 2U, prevCoefRs);
    cxt[i++] = hash(++n, coef, advPred[1] / 17, lcp[static_cast<uint64_t>(zu < zv)] / 24, lcp[2] / 20, lcp[3] / 24);
    cxt[i++] = hash(++n, coef, advPred[3] / 11, lcp[static_cast<uint64_t>(zu < zv)] / 50, lcp[2 + 3 * static_cast<int>(zu * zv > 1)] / 50,
                    lcp[3 + 3 * static_cast<int>(zu * zv > 1)] / 50);
    assert(i == N);
  }

  // predict next bit
  m1->add(128); //network bias
  sm.subscribe();
  assert(hbCount <= 2);
  switch( hbCount ) {
    case 0: {
      for( int i = 0; i < N; ++i ) {
        cp[i] = t[cxt[i]] + 1;
        const uint8_t s = *cp[i];
        const uint32_t p = sm.p2(i, s);
        m.add((static_cast<int>(p) - 2048) >> 3U);
        const int st = stretch(p);
        m1->add(st);
        m.add(st >> 1U);
      }
      break;
    }
    case 1: {
      int hc = 1U + (huffcode & 1U) * 3;
      for( int i = 0; i < N; ++i ) {
        cp[i] += hc;
        const uint8_t s = *cp[i];
        const uint32_t p = sm.p2(i, s);
        m.add((static_cast<int>(p) - 2048) >> 3U);
        const int st = stretch(p);
        m1->add(st);
        m.add(st >> 1U);
      }
      break;
    }
    default: {
      int hc = 1U + (huffcode & 1U);
      for( int i = 0; i < N; ++i ) {
        cp[i] += hc;
        const uint8_t s = *cp[i];
        const uint32_t p = sm.p2(i, s);
        m.add((static_cast<int>(p) - 2048) >> 3U);
        const int st = stretch(p);
        m1->add(st);
        m.add(st >> 1U);
      }
      break;
    }
  }
  if( hbCount == 0 ) {
    MJPEGMap.set(hash(mcuPos, column, row, hc >> 2U));
  }
  MJPEGMap.mix(*m1);
  m1->set(static_cast<uint32_t>(firstCol), 2);
  m1->set(coef | (min(3, huffBits) << 8), 1024);
  m1->set(((hc & 0x1FEU) << 1) | min(3, ilog2(zu + zv)), 1024);
  int pr = m1->p();
  m.add(stretch(pr) >> 1U);
  m.add((pr >> 2U) - 511);
  pr = apm1.p(pr, (hc & 511U) | (((advPred[1] / 16) & 63U) << 9U), 1023);
  m.add(stretch(pr) >> 1U);
  m.add((pr >> 2U) - 511);
  pr = apm2.p(pr, (hc & 511U) | (coef << 9U), 1023);
  m.add(stretch(pr) >> 1U);
  m.add((pr >> 2U) - 511);

  m.set(1 + (static_cast<int>(zu + zv < 5) | (static_cast<int>(huffBits > 8) << 1U) | (static_cast<int>(firstCol) << 2U)), 1 + 8);
  m.set(1 + ((hc & 0xFFU) | (min(3, (zu + zv) / 3)) << 8U), 1 + 1024);
  m.set(coef | (min(3, huffBits / 2) << 8), 1024);

  return 1;
}
