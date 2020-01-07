#ifndef PAQ8PX_JPEGMODEL_HPP
#define PAQ8PX_JPEGMODEL_HPP

// TODO: update this documentation
// Model JPEG. Return 1 if a JPEG file is detected or else 0.
// Only the baseline and 8 bit extended Huffman coded DCT modes are
// supported.  The model partially decodes the JPEG image to provide
// context for the Huffman coded symbols.

// Print a JPEG segment at buf[p...] for debugging
/*
void dump(const char* msg, int p) {
  printf("%s:", msg);
  int len=buf[p+2]*256+buf[p+3];
  for (int i=0; i<len+2; ++i)
    printf(" %02X", buf[p+i]);
  printf("\n");
}
*/

#define FINISH(success) \
  { \
    uint32_t length = pos - images[idx].offset; \
    /*if (success && idx && pos-lastPos==1)*/ \
    /*printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bEmbedded JPEG at offset %d, size: %d bytes, level %d\nCompressing... ", images[idx].offset-pos+blpos, length, idx), fflush(stdout);*/ \
    memset(&images[idx], 0, sizeof(JPEGImage)); \
    mcusize = 0, dqt_state = -1; \
    idx -= (idx > 0); \
    if (images[idx].app <= length) \
      images[idx].app = 0; \
    else \
      images[idx].app -= length; \
  }

// Detect invalid JPEG data.  The proper response is to silently
// fall back to a non-JPEG model.
#define JASSERT(x) \
  if (! (x)) { \
    /*  printf("JPEG error at %d, line %d: %s\n", pos, __LINE__, #x); */ \
    if (idx > 0) \
      FINISH(false) else images[idx].jpeg = 0; \
    return images[idx].nextJpeg; \
  }

struct HUF {
    uint32_t min, max;
    int val;
}; // Huffman decode tables
// huf[Tc][Th][m] is the minimum, maximum+1, and pointer to codes for
// coefficient type Tc (0=DC, 1=AC), table Th (0-3), length m+1 (m=0-15)

struct JPEGImage {
    uint32_t offset, // offset of SOI marker
            jpeg, // 1 if JPEG is header detected, 2 if image data
            nextJpeg, // updated with jpeg on next byte boundary
            app, // Bytes remaining to skip in this marker
            sof, sos, data, // pointers to buf
            htSize; // number of pointers in ht
    int ht[8]; // pointers to Huffman table headers
    uint8_t qTable[256]; // table
    int qMap[10]; // block -> table number
};

class JpegModel {
private:
    static constexpr int N = 32; // number of contexts
public:
    static constexpr int MIXERINPUTS = 2 * N + 6;
    static constexpr int MIXERCONTEXTS = (1 + 8) + (1 + 1024) + 1024;
    static constexpr int MIXERCONTEXTSETS = 6;

private:
    const Shared *const shared;
    Random rnd;
    // state of parser
    enum {
        SOF0 = 0xc0, SOF1, SOF2, SOF3, DHT, RST0 = 0xd0, SOI = 0xd8, EOI, SOS, DQT, DNL, DRI, APP0 = 0xe0, COM = 0xfe, FF
    }; // Second byte of 2 byte codes
    static const int MaxEmbeddedLevel = 3;
    JPEGImage images[MaxEmbeddedLevel] {};
    int idx = -1;
    uint32_t lastPos = 0;

    // Huffman decode state
    uint32_t huffcode = 0; // Current Huffman code including extra bits
    int huffbits = 0; // Number of valid bits in huffcode
    int huffsize = 0; // Number of bits without extra bits
    int rs = -1; // Decoded huffcode without extra bits.  It represents
    // 2 packed 4-bit numbers, r=run of zeros, s=number of extra bits for
    // first nonzero code.  huffcode is complete when rs >= 0.
    // rs is -1 prior to decoding incomplete huffcode.
    int hbcount = 2;

    int mcupos = 0; // position in MCU (0-639).  The low 6 bits mark
    // the coefficient in zigzag scan order (0=DC, 1-63=AC).  The high
    // bits mark the block within the MCU, used to select Huffman tables.

    // decoding tables
    Array<HUF> huf {128}; // Tc*64+Th*16+m -> min, max, val
    int mcusize = 0; // number of coefficients in an MCU
    int hufsel[2][10] {{0}}; // DC/AC, mcupos/64 -> huf decode table
    Array<uint8_t> hbuf {2048}; // Tc*1024+Th*256+hufcode -> RS

    // Image state
    Array<int> color {10}; // block -> component (0-3)
    Array<int> pred {4}; // component -> last DC value
    int dc = 0; // DC value of the current block
    int width = 0; // Image width in MCU
    int row = 0, column = 0; // in MCU (column 0 to width-1)
    RingBuffer<uint8_t> coefficientBuffer {0x20000}; // Rotating buffer of coefficients, coded as:
    // DC: level shifted absolute value, low 4 bits discarded, i.e.
    //   [-1023...1024] -> [0...255].
    // AC: as an RS code: a run of R (0-15) zeros followed by an s (0-15)
    //   bit number, or 00 for end of block (in zigzag order).
    //   However if R=0, then the format is ssss11xx where ssss is s,
    //   xx is the first 2 extra bits, and the last 2 bits are 1 (since
    //   this never occurs in a valid RS code).
    int cpos = 0; // position in coefficientBuffer
    int rs1 = 0; // last RS code
    int rstpos = 0, rstlen = 0; // reset position
    int ssum = 0, ssum1 = 0, ssum2 = 0, ssum3 = 0;
    // sum of s in RS codes in block and sum of s in first component

    RingBuffer<int> cbuf2 {0x20000};
    Array<int> advPred {4}, sumu {8}, sumv {8}, runPred {6};
    int prevCoef = 0, prevCoef2 = 0, prevCoefRs = 0;
    Array<int> ls {10}; // block -> distance to previous block
    Array<int> blockW {10}, blockN {10}, samplingFactors {4};
    Array<int> lcp {7}, zpos {64};

    //for parsing Quantization tables
    int dqt_state = -1;
    uint32_t dqtEnd = 0, qnum = 0;

    // context model
    BH<9> t; // context hash -> bit history
    // As a cache optimization, the context does not include the last 1-2
    // bits of huffcode if the length (huffbits) is not a multiple of 3.
    // The 7 mapped values are for context+{"", 0, 00, 01, 1, 10, 11}.
    Array<uint64_t> cxt {N}; // context hashes
    Array<uint8_t *> cp {N}; // context pointers
    IndirectMap MJPEGMap;
    StateMap sm;
    Mixer *m1;
    APM apm1, apm2;

public:
    JpegModel(const Shared *const sh, const uint64_t size) : shared(sh), t(size),
            MJPEGMap(sh, 21, 3, 128, 127), /* BitsOfContext, InputBits, Scale, Limit */
            sm(sh, N, 256, 1023, StateMap::BitHistory), apm1(sh, 0x8000, 24), apm2(sh, 0x20000, 24) {
      m1 = MixerFactory::createMixer(sh, N + 1, 2050, 3);
      m1->setScaleFactor(1024, 128);
    }

    ~JpegModel() {
      delete m1;
    }

    int mix(Mixer &m) {
      static constexpr uint8_t zzu[64] = {// zigzag coef -> u,v
              0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6,
              7, 7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 5, 6, 7, 7, 6, 7};
      static constexpr uint8_t zzv[64] = {0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4,
                                          5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 6, 7, 7};

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
      static constexpr uint8_t valuesAcChrominance[162] = {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51,
                                                           0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1,
                                                           0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24,
                                                           0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a,
                                                           0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
                                                           0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
                                                           0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82,
                                                           0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
                                                           0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
                                                           0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
                                                           0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
                                                           0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
                                                           0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};

      INJECT_SHARED_pos
      if( idx < 0 ) {
        memset(&images[0], 0, sizeof(images));
        idx = 0;
        lastPos = pos;
      }

      INJECT_SHARED_bpos
      INJECT_SHARED_buf

      // Be sure to quit on a byte boundary
      if( bpos == 0 )
        images[idx].nextJpeg = images[idx].jpeg > 1;
      if( bpos != 0 && !images[idx].jpeg )
        return images[idx].nextJpeg;
      if( bpos == 0 && images[idx].app > 0 ) {
        --images[idx].app;
        if( idx < MaxEmbeddedLevel && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
            ((buf(1) & 0xFE) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE)))
          memset(&images[++idx], 0, sizeof(JPEGImage));
      }
      if( images[idx].app > 0 )
        return images[idx].nextJpeg;
      if( bpos == 0 ) {
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
        if( !images[idx].jpeg && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
            ((buf(1) & 0xFE) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE))) {
          images[idx].jpeg = 1;
          images[idx].offset = pos - 4;
          images[idx].sos = images[idx].sof = images[idx].htSize = images[idx].data = 0, images[idx].app = (buf(1) >> 4 == 0xE) * 2;
          mcusize = huffcode = huffbits = huffsize = mcupos = cpos = 0, rs = -1;
          memset(&huf[0], 0, sizeof(huf));
          memset(&pred[0], 0, pred.size() * sizeof(int));
          rstpos = rstlen = 0;
        }

        // Detect end of JPEG when data contains a marker other than RSTx
        // or byte stuff (00), or if we jumped in position since the last byte seen
        if( images[idx].jpeg && images[idx].data && ((buf(2) == FF && buf(1) && (buf(1) & 0xf8) != RST0) || (pos - lastPos > 1))) {
          JASSERT((buf(1) == EOI) || (pos - lastPos > 1))
          FINISH(true)
        }
        lastPos = pos;
        if( !images[idx].jpeg )
          return images[idx].nextJpeg;

        // Detect APPx, COM or other markers, so we can skip them
        if( !images[idx].data && !images[idx].app && buf(4) == FF &&
            (((buf(3) > 0xC1) && (buf(3) <= 0xCF) && (buf(3) != DHT)) || ((buf(3) >= 0xDC) && (buf(3) <= 0xFE)))) {
          images[idx].app = buf(2) * 256 + buf(1) + 2;
          if( idx > 0 )
            JASSERT(pos + images[idx].app < images[idx].offset + images[idx - 1].app)
        }

        // Save pointers to sof, ht, sos, data,
        if( buf(5) == FF && buf(4) == SOS ) {
          int len = buf(3) * 256 + buf(2);
          if( len == 6 + 2 * buf(1) && buf(1) && buf(1) <= 4 ) // buf(1) is Ns
            images[idx].sos = pos - 5, images[idx].data = images[idx].sos + len + 2, images[idx].jpeg = 2;
        }
        if( buf(4) == FF && buf(3) == DHT && images[idx].htSize < 8 )
          images[idx].ht[images[idx].htSize++] = pos - 4;
        if( buf(4) == FF && (buf(3) & 0xFE) == SOF0 )
          images[idx].sof = pos - 4;

        // Parse quantization tables
        if( buf(4) == FF && buf(3) == DQT )
          dqtEnd = pos + buf(2) * 256 + buf(1) - 1, dqt_state = 0;
        else if( dqt_state >= 0 ) {
          if( pos >= dqtEnd )
            dqt_state = -1;
          else {
            if( dqt_state % 65 == 0 )
              qnum = buf(1);
            else {
              JASSERT(buf(1) > 0)
              JASSERT(qnum >= 0 && qnum < 4)
              images[idx].qTable[qnum * 64 + ((dqt_state % 65) - 1)] = buf(1) - 1;
            }
            dqt_state++;
          }
        }

        // Restart
        if( buf(2) == FF && (buf(1) & 0xf8) == RST0 ) {
          huffcode = huffbits = huffsize = mcupos = 0, rs = -1;
          memset(&pred[0], 0, pred.size() * sizeof(int));
          rstlen = column + row * width - rstpos;
          rstpos = column + row * width;
        }
      }

      {
        // Build Huffman tables
        // huf[Tc][Th][m] = min, max+1 codes of length m, pointer to byte values
        if( pos == images[idx].data && bpos == 1 ) {
          for( uint32_t i = 0; i < images[idx].htSize; ++i ) {
            uint32_t p = images[idx].ht[i] + 4; // pointer to current table after length field
            uint32_t end = p + buf[p - 2] * 256 + buf[p - 1] - 2; // end of Huffman table
            uint32_t count = 0; // sanity check
            while( p < end && end < pos && end < p + 2100 && ++count < 10 ) {
              int tc = buf[p] >> 4, th = buf[p] & 15;
              if( tc >= 2 || th >= 4 )
                break;
              JASSERT(tc >= 0 && tc < 2 && th >= 0 && th < 4)
              HUF *h = &huf[tc * 64 + th * 16]; // [tc][th][0];
              int val = p + 17; // pointer to values
              int hval = tc * 1024 + th * 256; // pointer to RS values in hbuf
              int j;
              for( j = 0; j < 256; ++j ) // copy RS codes
                hbuf[hval + j] = buf[val + j];
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
          huffcode = huffbits = huffsize = 0, rs = -1;

          // load default tables
          if( !images[idx].htSize ) {
            for( int tc = 0; tc < 2; tc++ ) {
              for( int th = 0; th < 2; th++ ) {
                HUF *h = &huf[tc * 64 + th * 16];
                int hval = tc * 1024 + th * 256;
                int code = 0, c = 0, x = 0;

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

                  hbuf[hval + c] = x;
                  c--;
                }
              }
            }
            images[idx].htSize = 4;
          }

          // Build Huffman table selection table (indexed by mcupos).
          // Get image width.
          if( !images[idx].sof && images[idx].sos )
            return images[idx].nextJpeg;
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
                if( hv >> 4 > hmax )
                  hmax = hv >> 4;
                hv = (hv & 15) * (hv >> 4); // number of blocks in component c
                JASSERT(hv >= 1 && hv + mcusize <= 10)
                while( hv ) {
                  JASSERT(mcusize < 10)
                  hufsel[0][mcusize] = buf[images[idx].sos + 2 * i + 6] >> 4 & 15;
                  hufsel[1][mcusize] = buf[images[idx].sos + 2 * i + 6] & 15;
                  JASSERT(hufsel[0][mcusize] < 4 && hufsel[1][mcusize] < 4)
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
          int j;
          for( j = 0; j < mcusize; ++j ) {
            ls[j] = 0;
            for( int i = 1; i < mcusize; ++i )
              if( color[(j + i) % mcusize] == color[j] )
                ls[j] = i;
            ls[j] = (mcusize - ls[j]) << 6;
          }
          for( j = 0; j < 64; ++j )
            zpos[zzu[j] + 8 * zzv[j]] = j;
          width = buf[images[idx].sof + 7] * 256 + buf[images[idx].sof + 8]; // in pixels
          width = (width - 1) / (hmax * 8) + 1; // in MCU
          JASSERT(width > 0)
          mcusize *= 64; // coefficients per MCU
          row = column = 0;

          // we can have more blocks than components then we have subsampling
          int x = 0, y = 0;
          for( j = 0; j < (mcusize >> 6); j++ ) {
            int i = color[j];
            int w = samplingFactors[i] >> 4, h = samplingFactors[i] & 0xf;
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
        if( mcusize && buf(1 + (bpos == 0)) != FF ) { // skip stuffed byte
          JASSERT(huffbits <= 32)
          INJECT_SHARED_y
          huffcode += huffcode + y;
          ++huffbits;
          if( rs < 0 ) {
            JASSERT(huffbits >= 1 && huffbits <= 16)
            const int ac = (mcupos & 63) > 0;
            JASSERT(mcupos >= 0 && (mcupos >> 6) < 10)
            JASSERT(ac == 0 || ac == 1)
            const int sel = hufsel[ac][mcupos >> 6];
            JASSERT(sel >= 0 && sel < 4)
            const int i = huffbits - 1;
            JASSERT(i >= 0 && i < 16)
            const HUF *h = &huf[ac * 64 + sel * 16]; // [ac][sel];
            JASSERT(h[i].min <= h[i].max && h[i].val < 2048 && huffbits > 0)
            if( huffcode < h[i].max ) {
              JASSERT(huffcode >= h[i].min)
              int k = h[i].val + huffcode - h[i].min;
              JASSERT(k >= 0 && k < 2048)
              rs = hbuf[k];
              huffsize = huffbits;
            }
          }
          if( rs >= 0 ) {
            if( huffsize + (rs & 15) == huffbits ) { // done decoding
              rs1 = rs;
              int ex = 0; // decoded extra bits
              if( mcupos & 63 ) { // AC
                if( rs == 0 ) { // EOB
                  mcupos = (mcupos + 63) & -64;
                  JASSERT(mcupos >= 0 && mcupos <= mcusize && mcupos <= 640)
                  while( cpos & 63 ) {
                    cbuf2.set(cpos, 0);
                    coefficientBuffer.set(cpos, (!rs) ? 0 : (63 - (cpos & 63)) << 4);
                    cpos++;
                    rs++;
                  }
                } else { // rs = r zeros + s extra bits for the next nonzero value
                  // If first extra bit is 0 then value is negative.
                  JASSERT((rs & 15U) <= 10)
                  const int r = rs >> 4U;
                  const int s = rs & 15U;
                  JASSERT(mcupos >> 6U == (mcupos + r) >> 6)
                  mcupos += r + 1;
                  ex = huffcode & ((1U << s) - 1);
                  if( s != 0 && (ex >> (s - 1)) == 0 )
                    ex -= (1U << s) - 1;
                  for( int i = r; i >= 1; --i ) {
                    cbuf2.set(cpos, 0);
                    coefficientBuffer.set(cpos, (i << 4U) | s);
                    cpos++;
                  }
                  cbuf2.set(cpos, ex);
                  coefficientBuffer.set(cpos, (s << 4U) | (huffcode << 2U >> s & 3U) | 12);
                  cpos++;
                  ssum += s;
                }
              } else { // DC: rs = 0S, s<12
                JASSERT(rs < 12)
                ++mcupos;
                ex = huffcode & ((1U << rs) - 1);
                if( rs != 0 && (ex >> (rs - 1)) == 0 )
                  ex -= (1U << rs) - 1;
                JASSERT(mcupos >= 0 && mcupos >> 6U < 10)
                const int comp = color[mcupos >> 6U];
                JASSERT(comp >= 0 && comp < 4)
                dc = pred[comp] += ex;
                JASSERT((cpos & 63) == 0)
                cbuf2.set(cpos, dc);
                coefficientBuffer.set(cpos, (dc + 1023) >> 3);
                cpos++;
                if((mcupos >> 6) == 0 ) {
                  ssum1 = 0;
                  ssum2 = ssum3;
                } else {
                  if( color[(mcupos >> 6) - 1] == color[0] )
                    ssum1 += (ssum3 = ssum);
                  ssum2 = ssum1;
                }
                ssum = rs;
              }
              JASSERT(mcupos >= 0 && mcupos <= mcusize)
              if( mcupos >= mcusize ) {
                mcupos = 0;
                if( ++column == width )
                  column = 0, ++row;
              }
              huffcode = huffsize = huffbits = 0, rs = -1;

              // UPDATE_ADV_PRED !!!!
              {
                const int acomp = mcupos >> 6, q = 64 * images[idx].qMap[acomp];
                const int zz = mcupos & 63, cpos_dc = cpos - zz;
                const bool norst = rstpos != column + row * width;
                if( zz == 0 ) {
                  for( int i = 0; i < 8; ++i )
                    sumu[i] = sumv[i] = 0;
                  // position in the buffer of first (DC) coefficient of the block
                  // of this same component that is to the west of this one, not
                  // necessarily in this MCU
                  int offsetDcW = cpos_dc - blockW[acomp];
                  // position in the buffer of first (DC) coefficient of the block
                  // of this same component that is to the north of this one, not
                  // necessarily in this MCU
                  int offsetDcN = cpos_dc - blockN[acomp];
                  for( int i = 0; i < 64; ++i ) {
                    sumu[zzu[i]] += (zzv[i] & 1 ? -1 : 1) * (zzv[i] ? 16 * (16 + zzv[i]) : 185) * (images[idx].qTable[q + i] + 1) *
                                    cbuf2[offsetDcN + i];
                    sumv[zzv[i]] += (zzu[i] & 1 ? -1 : 1) * (zzu[i] ? 16 * (16 + zzu[i]) : 185) * (images[idx].qTable[q + i] + 1) *
                                    cbuf2[offsetDcW + i];
                  }
                } else {
                  sumu[zzu[zz - 1]] -= (zzv[zz - 1] ? 16 * (16 + zzv[zz - 1]) : 185) * (images[idx].qTable[q + zz - 1] + 1) * cbuf2[cpos - 1];
                  sumv[zzv[zz - 1]] -= (zzu[zz - 1] ? 16 * (16 + zzu[zz - 1]) : 185) * (images[idx].qTable[q + zz - 1] + 1) * cbuf2[cpos - 1];
                }

                for( int i = 0; i < 3; ++i ) {
                  runPred[i] = runPred[i + 3] = 0;
                  for( int st = 0; st < 10 && zz + st < 64; ++st ) {
                    const int zz2 = zz + st;
                    int p = sumu[zzu[zz2]] * i + sumv[zzv[zz2]] * (2 - i);
                    p /= (images[idx].qTable[q + zz2] + 1) * 185 * (16 + zzv[zz2]) * (16 + zzu[zz2]) / 128;
                    if( zz2 == 0 && (norst || ls[acomp] == 64))
                      p -= cbuf2[cpos_dc - ls[acomp]];
                    p = (p < 0 ? -1 : +1) * ilog(abs(p) + 1);
                    if( st == 0 ) {
                      advPred[i] = p;
                    } else if( abs(p) > abs(advPred[i]) + 2 && abs(advPred[i]) < 210 ) {
                      if( runPred[i] == 0 )
                        runPred[i] = st * 2 + (p > 0);
                      if( abs(p) > abs(advPred[i]) + 21 && runPred[i + 3] == 0 )
                        runPred[i + 3] = st * 2 + (p > 0);
                    }
                  }
                }
                ex = 0;
                for( int i = 0; i < 8; ++i )
                  ex += (zzu[zz] < i) * sumu[i] + (zzv[zz] < i) * sumv[i];
                ex = (sumu[zzu[zz]] * (2 + zzu[zz]) + sumv[zzv[zz]] * (2 + zzv[zz]) - ex * 2) * 4 / (zzu[zz] + zzv[zz] + 16);
                ex /= (images[idx].qTable[q + zz] + 1) * 185;
                if( zz == 0 && (norst || ls[acomp] == 64))
                  ex -= cbuf2[cpos_dc - ls[acomp]];
                advPred[3] = (ex < 0 ? -1 : +1) * ilog(abs(ex) + 1);

                for( int i = 0; i < 4; ++i ) {
                  const int a = (i & 1 ? zzv[zz] : zzu[zz]), b = (i & 2 ? 2 : 1);
                  if( a < b )
                    ex = 65535;
                  else {
                    const int zz2 = zpos[zzu[zz] + 8 * zzv[zz] - (i & 1 ? 8 : 1) * b];
                    ex = (images[idx].qTable[q + zz2] + 1) * cbuf2[cpos_dc + zz2] / (images[idx].qTable[q + zz] + 1);
                    ex = (ex < 0 ? -1 : +1) * (ilog(abs(ex) + 1) + (ex != 0 ? 17 : 0));
                  }
                  lcp[i] = ex;
                }
                if((zzu[zz] * zzv[zz]) != 0 ) {
                  const int zz2 = zpos[zzu[zz] + 8 * zzv[zz] - 9];
                  ex = (images[idx].qTable[q + zz2] + 1) * cbuf2[cpos_dc + zz2] / (images[idx].qTable[q + zz] + 1);
                  lcp[4] = (ex < 0 ? -1 : +1) * (ilog(abs(ex) + 1) + (ex != 0 ? 17 : 0));

                  ex = (images[idx].qTable[q + zpos[8 * zzv[zz]]] + 1) * cbuf2[cpos_dc + zpos[8 * zzv[zz]]] / (images[idx].qTable[q + zz] + 1);
                  lcp[5] = (ex < 0 ? -1 : +1) * (ilog(abs(ex) + 1) + (ex != 0 ? 17 : 0));

                  ex = (images[idx].qTable[q + zpos[zzu[zz]]] + 1) * cbuf2[cpos_dc + zpos[zzu[zz]]] / (images[idx].qTable[q + zz] + 1);
                  lcp[6] = (ex < 0 ? -1 : +1) * (ilog(abs(ex) + 1) + (ex != 0 ? 17 : 0));
                } else
                  lcp[4] = lcp[5] = lcp[6] = 65535;

                int prev1 = 0, prev2 = 0, cnt1 = 0, cnt2 = 0, r = 0, s = 0;
                prevCoefRs = coefficientBuffer[cpos - 64];
                for( int i = 0; i < acomp; i++ ) {
                  ex = 0;
                  ex += cbuf2[cpos - (acomp - i) * 64];
                  if( zz == 0 && (norst || ls[i] == 64))
                    ex -= cbuf2[cpos_dc - (acomp - i) * 64 - ls[i]];
                  if( color[i] == color[acomp] - 1 ) {
                    prev1 += ex;
                    cnt1++;
                    r += coefficientBuffer[cpos - (acomp - i) * 64] >> 4;
                    s += coefficientBuffer[cpos - (acomp - i) * 64] & 0xF;
                  }
                  if( color[acomp] > 1 && color[i] == color[0] ) {
                    prev2 += ex;
                    cnt2++;
                  }
                }
                if( cnt1 > 0 )
                  prev1 /= cnt1, r /= cnt1, s /= cnt1, prevCoefRs = (r << 4) | s;
                if( cnt2 > 0 )
                  prev2 /= cnt2;
                prevCoef = (prev1 < 0 ? -1 : +1) * ilog(11 * abs(prev1) + 1) + (cnt1 << 20);
                prevCoef2 = (prev2 < 0 ? -1 : +1) * ilog(11 * abs(prev2) + 1);

                if( column == 0 && blockW[acomp] > 64 * acomp )
                  runPred[1] = runPred[2], runPred[0] = 0, advPred[1] = advPred[2], advPred[0] = 0;
                if( row == 0 && blockN[acomp] > 64 * acomp )
                  runPred[1] = runPred[0], runPred[2] = 0, advPred[1] = advPred[0], advPred[2] = 0;
              } // !!!!
            }
          }
        }
      }

      // Estimate next bit probability
      if( !images[idx].jpeg || !images[idx].data )
        return images[idx].nextJpeg;
      if( buf(1 + (bpos == 0)) == FF ) {
        m.add(128); //network bias
        m.set(0, 1 + 8);
        m.set(0, 1 + 1024);
        m.set(buf(1), 1024);
        return true;
      }
      if( rstlen > 0 && rstlen == column + row * width - rstpos && mcupos == 0 && (int) huffcode == (1 << huffbits) - 1 ) {
        m.add(2047); //network bias
        m.set(0, 1 + 8);
        m.set(0, 1 + 1024);
        m.set(buf(1), 1024);
        return true;
      }

      // update model
      if( cp[N - 1] ) {
        INJECT_SHARED_y
        for( int i = 0; i < N; ++i )
          StateTable::update(cp[i], y, rnd);
      }

      // update context
      const int comp = color[mcupos >> 6];
      const int coef = (mcupos & 63) | comp << 6;
      const int hc = (huffcode * 4 + ((mcupos & 63) == 0) * 2 + (comp == 0)) | 1 << (huffbits + 2);
      const bool firstcol = column == 0 && blockW[mcupos >> 6] > mcupos;
      if( ++hbcount > 2 || huffbits == 0 )
        hbcount = 0;
      JASSERT(coef >= 0 && coef < 256)
      const int zu = zzu[mcupos & 63], zv = zzv[mcupos & 63];
      if( hbcount == 0 ) {
        uint64_t n = hc * 32;
        int i = 0;
        cxt[i++] = hash(++n, coef, advPred[2] / 12 + (runPred[2] << 8), ssum2 >> 6, prevCoef / 72);
        cxt[i++] = hash(++n, coef, advPred[0] / 12 + (runPred[0] << 8), ssum2 >> 6, prevCoef / 72);
        cxt[i++] = hash(++n, coef, advPred[1] / 11 + (runPred[1] << 8), ssum2 >> 6);
        cxt[i++] = hash(++n, rs1, advPred[2] / 7, runPred[5] / 2, prevCoef / 10);
        cxt[i++] = hash(++n, rs1, advPred[0] / 7, runPred[3] / 2, prevCoef / 10);
        cxt[i++] = hash(++n, rs1, advPred[1] / 11, runPred[4]);
        cxt[i++] = hash(++n, advPred[2] / 14, runPred[2], advPred[0] / 14, runPred[0]);
        cxt[i++] = hash(++n, coefficientBuffer[cpos - blockN[mcupos >> 6]] >> 4, advPred[3] / 17, runPred[1], runPred[5]);
        cxt[i++] = hash(++n, coefficientBuffer[cpos - blockW[mcupos >> 6]] >> 4, advPred[3] / 17, runPred[1], runPred[3]);
        cxt[i++] = hash(++n, lcp[0] / 22, lcp[1] / 22, advPred[1] / 7, runPred[1]);
        cxt[i++] = hash(++n, lcp[0] / 22, lcp[1] / 22, mcupos & 63, lcp[4] / 30);
        cxt[i++] = hash(++n, zu / 2, lcp[0] / 13, lcp[2] / 30, prevCoef / 40 + ((prevCoef2 / 28) << 20));
        cxt[i++] = hash(++n, zv / 2, lcp[1] / 13, lcp[3] / 30, prevCoef / 40 + ((prevCoef2 / 28) << 20));
        cxt[i++] = hash(++n, rs1, prevCoef / 42, prevCoef2 / 34, lcp[0] / 60, lcp[2] / 14, lcp[1] / 60, lcp[3] / 14);
        cxt[i++] = hash(++n, mcupos & 63, column >> 1);
        cxt[i++] = hash(++n, column >> 3, min(5 + 2 * (!comp), zu + zv), lcp[0] / 10, lcp[2] / 40, lcp[1] / 10, lcp[3] / 40);
        cxt[i++] = hash(++n, ssum >> 3, mcupos & 63);
        cxt[i++] = hash(++n, rs1, mcupos & 63, runPred[1]);
        cxt[i++] = hash(++n, coef, ssum2 >> 5, advPred[3] / 30,
                        (comp) ? hash(prevCoef / 22, prevCoef2 / 50) : ssum / ((mcupos & 0x3F) + 1));
        cxt[i++] = hash(++n, lcp[0] / 40, lcp[1] / 40, advPred[1] / 28, (comp) ? prevCoef / 40 + ((prevCoef2 / 40) << 20) : lcp[4] / 22,
                        min(7, zu + zv), ssum / (2 * (zu + zv) + 1));
        cxt[i++] = hash(++n, zv, coefficientBuffer[cpos - blockN[mcupos >> 6]], advPred[2] / 28, runPred[2]);
        cxt[i++] = hash(++n, zu, coefficientBuffer[cpos - blockW[mcupos >> 6]], advPred[0] / 28, runPred[0]);
        cxt[i++] = hash(++n, advPred[2] / 7, runPred[2]);
        cxt[i++] = hash(n, advPred[0] / 7, runPred[0]);
        cxt[i++] = hash(n, advPred[1] / 7, runPred[1]);
        cxt[i++] = hash(++n, zv, lcp[1] / 14, advPred[2] / 16, runPred[5]);
        cxt[i++] = hash(++n, zu, lcp[0] / 14, advPred[0] / 16, runPred[3]);
        cxt[i++] = hash(++n, lcp[0] / 14, lcp[1] / 14, advPred[3] / 16);
        cxt[i++] = hash(++n, coef, prevCoef / 10, prevCoef2 / 20);
        cxt[i++] = hash(++n, coef, ssum >> 2, prevCoefRs);
        cxt[i++] = hash(++n, coef, advPred[1] / 17, lcp[(zu < zv)] / 24, lcp[2] / 20, lcp[3] / 24);
        cxt[i++] = hash(++n, coef, advPred[3] / 11, lcp[(zu < zv)] / 50, lcp[2 + 3 * (zu * zv > 1)] / 50, lcp[3 + 3 * (zu * zv > 1)] / 50);
        assert(i == N);
      }

      // predict next bit
      m1->add(128); //network bias
      sm.subscribe();
      assert(hbcount <= 2);
      switch( hbcount ) {
        case 0: {
          for( int i = 0; i < N; ++i ) {
            cp[i] = t[cxt[i]] + 1;
            const uint8_t s = *cp[i];
            const uint32_t p = sm.p2(i, s);
            m.add(((int) p - 2048) >> 3);
            const int st = stretch(p);
            m1->add(st);
            m.add(st >> 1);
          }
          break;
        }
        case 1: {
          int hc = 1 + (huffcode & 1) * 3;
          for( int i = 0; i < N; ++i ) {
            cp[i] += hc;
            const uint8_t s = *cp[i];
            const uint32_t p = sm.p2(i, s);
            m.add(((int) p - 2048) >> 3);
            const int st = stretch(p);
            m1->add(st);
            m.add(st >> 1);
          }
          break;
        }
        default: {
          int hc = 1 + (huffcode & 1);
          for( int i = 0; i < N; ++i ) {
            cp[i] += hc;
            const uint8_t s = *cp[i];
            const uint32_t p = sm.p2(i, s);
            m.add(((int) p - 2048) >> 3);
            const int st = stretch(p);
            m1->add(st);
            m.add(st >> 1);
          }
          break;
        }
      }
      if( !hbcount )
        MJPEGMap.set(hash(mcupos, column, row, hc >> 2));
      MJPEGMap.mix(*m1);
      m1->set(firstcol, 2);
      m1->set(coef | (min(3, huffbits) << 8), 1024);
      m1->set(((hc & 0x1FE) << 1) | min(3, ilog2(zu + zv)), 1024);
      int pr = m1->p();
      m.add(stretch(pr) >> 1);
      m.add((pr >> 2) - 511);
      pr = apm1.p(pr, (hc & 511) | (((advPred[1] / 16) & 63) << 9), 1023);
      m.add(stretch(pr) >> 1);
      m.add((pr >> 2) - 511);
      pr = apm2.p(pr, (hc & 511) | (coef << 9), 1023);
      m.add(stretch(pr) >> 1);
      m.add((pr >> 2) - 511);

      m.set(1 + ((zu + zv < 5) | ((huffbits > 8) << 1) | (firstcol << 2)), 1 + 8);
      m.set(1 + ((hc & 0xFF) | (min(3, (zu + zv) / 3)) << 8), 1 + 1024);
      m.set(coef | (min(3, huffbits / 2) << 8), 1024);

      return true;
    }
};

#endif //PAQ8PX_JPEGMODEL_HPP
