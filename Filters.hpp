#ifndef PAQ8PX_FILTERS_HPP
#define PAQ8PX_FILTERS_HPP

#include "UTF8.hpp"

/////////////////////////// Filters /////////////////////////////////
//
// Before compression, data is encoded in blocks with the following format:
//
//   <type> <size> <encoded-data>
//
// type is 1 byte (type BlockType): DEFAULT=0, JPEG, EXE
// size is 4 bytes in big-endian format.
// Encoded-data decodes to <size> bytes.  The encoded size might be
// different.  Encoded data is designed to be more compressible.
//
//   void encode(File *in, File *out, int n);
//
// Reads n bytes of in (open in "rb" mode) and encodes one or
// more blocks to temporary file out (open in "wb+" mode).
// The file pointer of in is advanced n bytes.  The file pointer of
// out is positioned after the last byte written.
//
//   en.setFile(File *out);
//   int decode(Encoder& en);
//
// Decodes and returns one byte.  Input is from en.decompress(), which
// reads from out if in COMPRESS mode.  During compression, n calls
// to decode() must exactly match n bytes of in, or else it is compressed
// as type 0 without encoding.
//
//   BlockType detect(File *in, int n, BlockType type);
//
// Reads n bytes of in, and detects when the type changes to
// something else.  If it does, then the file pointer is repositioned
// to the start of the change and the new type is returned.  If the type
// does not change, then it repositions the file pointer n bytes ahead
// and returns the old type.
//
// For each type X there are the following 2 functions:
//
//   void encode_X(File *in, File *out, int n, ...);
//
// encodes n bytes from in to out.
//
//   int decode_X(Encoder& en);
//
// decodes one byte from en and returns it.  decode() and decode_X()
// maintain state information using static variables.


#define IMG_DET(type, start_pos, header_len, width, height) return dett = (type), \
                                                                   deth = (header_len), detd = (width) * (height), info = (width), \
                                                                   in->setpos(start + (start_pos)), HDR

#define AUD_DET(type, start_pos, header_len, data_len, wmode) return dett = (type), \
                                                                     deth = (header_len), detd = (data_len), info = (wmode), \
                                                                     in->setpos(start + (start_pos)), HDR


// Function ecc_compute(), edc_compute() and eccedc_init() taken from
// ** UNECM - Decoder for ECM (Error code Modeler) format.
// ** version 1.0
// ** Copyright (c) 2002 Neill Corlett

/* LUTs used for computing ECC/EDC */
static uint8_t ecc_f_lut[256];
static uint8_t ecc_b_lut[256];
static uint32_t edc_lut[256];
static int luts_init = 0;

void eccedc_init() {
  if( luts_init )
    return;
  uint32_t i, j, edc;
  for( i = 0; i < 256; i++ ) {
    j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
    ecc_f_lut[i] = j;
    ecc_b_lut[i ^ j] = i;
    edc = i;
    for( j = 0; j < 8; j++ )
      edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
    edc_lut[i] = edc;
  }
  luts_init = 1;
}

void ecc_compute(uint8_t *src, uint32_t major_count, uint32_t minor_count, uint32_t major_mult, uint32_t minor_inc, uint8_t *dest) {
  uint32_t size = major_count * minor_count;
  uint32_t major, minor;
  for( major = 0; major < major_count; major++ ) {
    uint32_t index = (major >> 1) * major_mult + (major & 1);
    uint8_t ecc_a = 0;
    uint8_t ecc_b = 0;
    for( minor = 0; minor < minor_count; minor++ ) {
      uint8_t temp = src[index];
      index += minor_inc;
      if( index >= size )
        index -= size;
      ecc_a ^= temp;
      ecc_b ^= temp;
      ecc_a = ecc_f_lut[ecc_a];
    }
    ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
    dest[major] = ecc_a;
    dest[major + major_count] = ecc_a ^ ecc_b;
  }
}

uint32_t edc_compute(const uint8_t *src, int size) {
  uint32_t edc = 0;
  while( size-- )
    edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
  return edc;
}

int expand_cd_sector(uint8_t *data, int address, int test) {
  uint8_t d2[2352];
  eccedc_init();
  //sync pattern: 00 FF FF FF FF FF FF FF FF FF FF 00
  d2[0] = d2[11] = 0;
  for( int i = 1; i < 11; i++ )
    d2[i] = 255;
  //determine Mode and Form
  int mode = (data[15] != 1 ? 2 : 1);
  int form = (data[15] == 3 ? 2 : 1);
  //address (Minutes, Seconds, Sectors)
  if( address == -1 )
    for( int i = 12; i < 15; i++ )
      d2[i] = data[i];
  else {
    int c1 = (address & 15) + ((address >> 4) & 15) * 10;
    int c2 = ((address >> 8) & 15) + ((address >> 12) & 15) * 10;
    int c3 = ((address >> 16) & 15) + ((address >> 20) & 15) * 10;
    c1 = (c1 + 1) % 75;
    if( c1 == 0 ) {
      c2 = (c2 + 1) % 60;
      if( c2 == 0 )
        c3++;
    }
    d2[12] = (c3 % 10) + 16 * (c3 / 10);
    d2[13] = (c2 % 10) + 16 * (c2 / 10);
    d2[14] = (c1 % 10) + 16 * (c1 / 10);
  }
  d2[15] = mode;
  if( mode == 2 )
    for( int i = 16; i < 24; i++ )
      d2[i] = data[i - 4 * (i >= 20)]; //8 byte subheader
  if( form == 1 ) {
    if( mode == 2 ) {
      d2[1] = d2[12], d2[2] = d2[13], d2[3] = d2[14];
      d2[12] = d2[13] = d2[14] = d2[15] = 0;
    } else {
      for( int i = 2068; i < 2076; i++ )
        d2[i] = 0; //Mode1: reserved 8 (zero) bytes
    }
    for( int i = 16 + 8 * (mode == 2); i < 2064 + 8 * (mode == 2); i++ )
      d2[i] = data[i]; //data bytes
    uint32_t edc = edc_compute(d2 + 16 * (mode == 2), 2064 - 8 * (mode == 2));
    for( int i = 0; i < 4; i++ )
      d2[2064 + 8 * (mode == 2) + i] = (edc >> (8 * i)) & 0xff;
    ecc_compute(d2 + 12, 86, 24, 2, 86, d2 + 2076);
    ecc_compute(d2 + 12, 52, 43, 86, 88, d2 + 2248);
    if( mode == 2 ) {
      d2[12] = d2[1], d2[13] = d2[2], d2[14] = d2[3], d2[15] = 2;
      d2[1] = d2[2] = d2[3] = 255;
    }
  }
  for( int i = 0; i < 2352; i++ )
    if( d2[i] != data[i] && test )
      form = 2;
  if( form == 2 ) {
    for( int i = 24; i < 2348; i++ )
      d2[i] = data[i]; //data bytes
    uint32_t edc = edc_compute(d2 + 16, 2332);
    for( int i = 0; i < 4; i++ )
      d2[2348 + i] = (edc >> (8 * i)) & 0xff; //EDC
  }
  for( int i = 0; i < 2352; i++ )
    if( d2[i] != data[i] && test )
      return 0;
    else
      data[i] = d2[i];
  return mode + form - 1;
}

#ifdef USE_ZLIB

int parse_zlib_header(int header) {
  switch( header ) {
    case 0x2815:
      return 0;
    case 0x2853:
      return 1;
    case 0x2891:
      return 2;
    case 0x28cf:
      return 3;
    case 0x3811:
      return 4;
    case 0x384f:
      return 5;
    case 0x388d:
      return 6;
    case 0x38cb:
      return 7;
    case 0x480d:
      return 8;
    case 0x484b:
      return 9;
    case 0x4889:
      return 10;
    case 0x48c7:
      return 11;
    case 0x5809:
      return 12;
    case 0x5847:
      return 13;
    case 0x5885:
      return 14;
    case 0x58c3:
      return 15;
    case 0x6805:
      return 16;
    case 0x6843:
      return 17;
    case 0x6881:
      return 18;
    case 0x68de:
      return 19;
    case 0x7801:
      return 20;
    case 0x785e:
      return 21;
    case 0x789c:
      return 22;
    case 0x78da:
      return 23;
  }
  return -1;
}

int zlib_inflateInit(z_streamp strm, int zh) {
  if( zh == -1 )
    return inflateInit2(strm, -MAX_WBITS);
  else
    return inflateInit(strm);
}

#endif //USE_ZLIB

bool IsGrayscalePalette(File *in, int n = 256, int isRGBA = 0) {
  uint64_t offset = in->curPos();
  int stride = 3 + isRGBA, res = (n > 0) << 8, order = 1;
  for( int i = 0; (i < n * stride) && (res >> 8); i++ ) {
    int b = in->getchar();
    if( b == EOF) {
      res = 0;
      break;
    }
    if( !i ) {
      res = 0x100 | b;
      order = 1 - 2 * (b > int(ilog2(n) / 4));
      continue;
    }
    //"j" is the index of the current byte in this color entry
    int j = i % stride;
    if( !j ) {
      // load first component of this entry
      int k = (b - (res & 0xFF)) * order;
      res = res & ((k >= 0 && k <= 8) << 8);
      res |= (res) ? b : 0;
    } else if( j == 3 )
      res &= ((!b || (b == 0xFF)) * 0x1FF); // alpha/attribute component must be zero or 0xFF
    else
      res &= ((b == (res & 0xFF)) * 0x1FF);
  }
  in->setpos(offset);
  return (res >> 8) > 0;
}


#define TEXT_MIN_SIZE 1500 // size of minimum allowed text block (in bytes)
#define TEXT_MAX_MISSES 2 // threshold: max allowed number of invalid UTF8 sequences seen recently before reporting "fail"
#define TEXT_ADAPT_RATE 256 // smaller (like 32) = illegal sequences are allowed to come more often, larger (like 1024) = more rigorous detection

struct TextParserStateInfo {
    Array<uint64_t> _start;
    Array<uint64_t> _end; // position of last char with a valid UTF8 state: marks the end of the detected TEXT block
    Array<uint32_t> _EOLType; // 0: none or CR-only;   1: CRLF-only (applicable to EOL transform);   2: mixed or LF-only
    uint32_t invalidCount; // adaptive count of invalid UTF8 sequences seen recently
    uint32_t UTF8State; // state of utf8 parser; 0: valid;  12: invalid;  any other value: yet uncertain (more bytes must be read)
    TextParserStateInfo() : _start(0), _end(0), _EOLType(0) {}

    void reset(uint64_t startpos) {
      _start.resize(1);
      _end.resize(1);
      _start[0] = startpos;
      _end[0] = startpos - 1;
      _EOLType.resize(1);
      _EOLType[0] = 0;
      invalidCount = 0;
      UTF8State = 0;
    }

    uint64_t start() { return _start[_start.size() - 1]; }

    uint64_t end() { return _end[_end.size() - 1]; }

    void setend(uint64_t end) { _end[_end.size() - 1] = end; }

    uint32_t EOLType() { return _EOLType[_EOLType.size() - 1]; }

    void setEOLType(uint32_t EOLType) { _EOLType[_EOLType.size() - 1] = EOLType; }

    uint64_t validlength() { return end() - start() + 1; }

    void next(uint64_t startpos) {
      _start.pushBack(startpos);
      _end.pushBack(startpos - 1);
      _EOLType.pushBack(0);
      invalidCount = 0;
      UTF8State = 0;
    }

    void removefirst() {
      if( _start.size() == 1 )
        reset(0);
      else {
        for( int i = 0; i < (int) _start.size() - 1; i++ ) {
          _start[i] = _start[i + 1];
          _end[i] = _end[i + 1];
          _EOLType[i] = _EOLType[i + 1];
        }
        _start.popBack();
        _end.popBack();
        _EOLType.popBack();
      }
    }
} textparser;


// Detect blocks
BlockType detect(File *in, uint64_t blockSize, BlockType type, int &info) {
  //TODO: Large file support
  int n = (int) blockSize;
  uint32_t buf3 = 0, buf2 = 0, buf1 = 0, buf0 = 0; // last 16 bytes
  uint64_t start = in->curPos();

  // For EXE detection
  Array<int> abspos(256), // CALL/JMP abs. addr. low byte -> last offset
          relpos(256); // CALL/JMP relative addr. low byte -> last offset
  int e8e9count = 0; // number of consecutive CALL/JMPs
  int e8e9pos = 0; // offset of first CALL or JMP instruction
  int e8e9last = 0; // offset of most recent CALL or JMP

  int soi = 0, sof = 0, sos = 0, app = 0; // For JPEG detection - position where found
#ifdef USE_AUDIOMODEL
  int wavi = 0, wavsize = 0, wavch = 0, wavbps = 0, wavm = 0, wavtype = 0, wavlen = 0, wavlist = 0; // For WAVE detection
  int aiff = 0, aiffm = 0, aiffs = 0; // For AIFF detection
  int s3mi = 0, s3mno = 0, s3mni = 0; // For S3M detection
#endif //  USE_AUDIOMODEL
  uint32_t bmpi = 0, dibi = 0, imgbpp = 0, bmpx = 0, bmpy = 0, bmpof = 0, bmps = 0, n_colors = 0; // For BMP detection
  int rgbi = 0, rgbx = 0, rgby = 0; // For RGB detection
  int tga = 0, tgax = 0, tgay = 0, tgaz = 0, tgat = 0, tgaid = 0, tgamap = 0; // For TGA detection
  // ascii images are currently not supported
  int pgm = 0, pgmcomment = 0, pgmw = 0, pgmh = 0, pgm_ptr = 0, pgmc = 0, pgmn = 0, pamatr = 0, pamd = 0, pgmdata = 0, pgmdatasize = 0; // For PBM, PGM, PPM, PAM detection
  char pgm_buf[32];
  int cdi = 0, cda = 0, cdm = 0; // For CD sectors detection
  uint32_t cdf = 0;
  unsigned char zbuf[256 + 32] = {0}, zin[1 << 16] = {0}, zout[1 << 16] = {0}; // For ZLIB stream detection
  int zbufpos = 0, zzippos = -1, histogram[256] = {0};
  int pdfim = 0, pdfimw = 0, pdfimh = 0, pdfimb = 0, pdfgray = 0, pdfimp = 0;
  int b64s = 0, b64i = 0, b64line = 0, b64nl = 0; // For base64 detection
  int gif = 0, gifa = 0, gifi = 0, gifw = 0, gifc = 0, gifb = 0, gifplt = 0; // For GIF detection
  static int gifgray = 0;
  int png = 0, pngw = 0, pngh = 0, pngbps = 0, pngtype = 0, pnggray = 0, lastchunk = 0, nextchunk = 0; // For PNG detection
  // For image detection
  static int deth = 0, detd = 0; // detected header/data size in bytes
  static BlockType dett; // detected block type
  if( deth ) {
    in->setpos(start + deth);
    deth = 0;
    return dett;
  } else if( detd ) {
    in->setpos(start + min(blockSize, (uint64_t) detd));
    detd = 0;
    return DEFAULT;
  }

  textparser.reset(0);
  for( int i = 0; i < n; ++i ) {
    int c = in->getchar();
    if( c == EOF)
      return (BlockType) (-1);
    buf3 = buf3 << 8 | buf2 >> 24;
    buf2 = buf2 << 8 | buf1 >> 24;
    buf1 = buf1 << 8 | buf0 >> 24;
    buf0 = buf0 << 8 | c;

    // detect PNG images
    if( !png && buf3 == 0x89504E47 /*%PNG*/ && buf2 == 0x0D0A1A0A && buf1 == 0x0000000D && buf0 == 0x49484452 )
      png = i, pngtype = -1, lastchunk = buf3;
    if( png ) {
      const int p = i - png;
      if( p == 12 ) {
        pngw = buf2;
        pngh = buf1;
        pngbps = buf0 >> 24;
        pngtype = (uint8_t) (buf0 >> 16);
        pnggray = 0;
        png *= ((buf0 & 0xFFFF) == 0 && pngw && pngh && pngbps == 8 &&
                (!pngtype || pngtype == 2 || pngtype == 3 || pngtype == 4 || pngtype == 6));
      } else if( p > 12 && pngtype < 0 )
        png = 0;
      else if( p == 17 ) {
        png *= ((buf1 & 0xFF) == 0);
        nextchunk = (png) ? i + 8 : 0;
      } else if( p > 17 && i == nextchunk ) {
        nextchunk += buf1 + 4 /*CRC*/ + 8 /*Chunk length+Id*/;
        lastchunk = buf0;
        png *= (lastchunk != 0x49454E44 /*IEND*/);
        if( lastchunk == 0x504C5445 /*PLTE*/) {
          png *= (buf1 % 3 == 0);
          pnggray = (png && IsGrayscalePalette(in, buf1 / 3));
        }
      }
    }

    // ZLIB stream detection
#ifdef USE_ZLIB
    histogram[c]++;
    if( i >= 256 )
      histogram[zbuf[zbufpos]]--;
    zbuf[zbufpos] = c;
    if( zbufpos < 32 )
      zbuf[zbufpos + 256] = c;
    zbufpos = (zbufpos + 1) & 0xFF;

    int zh = parse_zlib_header(((int) zbuf[(zbufpos - 32) & 0xFF]) * 256 + (int) zbuf[(zbufpos - 32 + 1) & 0xFF]);
    bool valid = (i >= 31 && zh != -1);
    if( !valid && options & OPTION_BRUTE && i >= 255 ) {
      uint8_t BTYPE = (zbuf[zbufpos] & 7) >> 1;
      if((valid = (BTYPE == 1 || BTYPE == 2))) {
        int maximum = 0, used = 0, offset = zbufpos;
        for( int i = 0; i < 4; i++, offset += 64 ) {
          for( int j = 0; j < 64; j++ ) {
            int freq = histogram[zbuf[(offset + j) & 0xFF]];
            used += (freq > 0);
            maximum += (freq > maximum);
          }
          if( maximum >= ((12 + i) << i) || used * (6 - i) < (i + 1) * 64 ) {
            valid = false;
            break;
          }
        }
      }
    }
    if( valid || zzippos == i ) {
      int streamLength = 0, ret = 0, brute = (zh == -1 && zzippos != i);

      // Quick check possible stream by decompressing first 32 bytes
      z_stream strm;
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.next_in = Z_NULL;
      strm.avail_in = 0;
      if( zlib_inflateInit(&strm, zh) == Z_OK ) {
        strm.next_in = &zbuf[(zbufpos - (brute ? 0 : 32)) & 0xFF];
        strm.avail_in = 32;
        strm.next_out = zout;
        strm.avail_out = 1 << 16;
        ret = inflate(&strm, Z_FINISH);
        ret = (inflateEnd(&strm) == Z_OK && (ret == Z_STREAM_END || ret == Z_BUF_ERROR) && strm.total_in >= 16);
      }
      if( ret ) {
        // Verify valid stream and determine stream length
        const uint64_t savedpos = in->curPos();
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.next_in = Z_NULL;
        strm.avail_in = 0;
        strm.total_in = strm.total_out = 0;
        if( zlib_inflateInit(&strm, zh) == Z_OK ) {
          for( int j = i - (brute ? 255 : 31); j < n; j += 1 << 16 ) {
            unsigned int blsize = min(n - j, 1 << 16);
            in->setpos(start + j);
            if( in->blockRead(zin, blsize) != blsize )
              break;
            strm.next_in = zin;
            strm.avail_in = blsize;
            do {
              strm.next_out = zout;
              strm.avail_out = 1 << 16;
              ret = inflate(&strm, Z_FINISH);
            } while( strm.avail_out == 0 && ret == Z_BUF_ERROR);
            if( ret == Z_STREAM_END )
              streamLength = strm.total_in;
            if( ret != Z_BUF_ERROR)
              break;
          }
          if( inflateEnd(&strm) != Z_OK )
            streamLength = 0;
        }
        in->setpos(savedpos);
      }
      if( streamLength > (brute << 7)) {
        info = 0;
        if( pdfimw > 0 && pdfimw < 0x1000000 && pdfimh > 0 ) {
          if( pdfimb == 8 && (int) strm.total_out == pdfimw * pdfimh )
            info = ((pdfgray ? IMAGE8GRAY : IMAGE8) << 24) | pdfimw;
          if( pdfimb == 8 && (int) strm.total_out == pdfimw * pdfimh * 3 )
            info = (IMAGE24 << 24) | pdfimw * 3;
          if( pdfimb == 4 && (int) strm.total_out == ((pdfimw + 1) / 2) * pdfimh )
            info = (IMAGE4 << 24) | ((pdfimw + 1) / 2);
          if( pdfimb == 1 && (int) strm.total_out == ((pdfimw + 7) / 8) * pdfimh )
            info = (IMAGE1 << 24) | ((pdfimw + 7) / 8);
          pdfgray = 0;
        } else if( png && pngw < 0x1000000 && lastchunk == 0x49444154 /*IDAT*/) {
          if( pngbps == 8 && pngtype == 2 && (int) strm.total_out == (pngw * 3 + 1) * pngh )
            info = (PNG24 << 24) | (pngw * 3), png = 0;
          else if( pngbps == 8 && pngtype == 6 && (int) strm.total_out == (pngw * 4 + 1) * pngh )
            info = (PNG32 << 24) | (pngw * 4), png = 0;
          else if( pngbps == 8 && (!pngtype || pngtype == 3) && (int) strm.total_out == (pngw + 1) * pngh )
            info = (((!pngtype || pnggray) ? PNG8GRAY : PNG8) << 24) | (pngw), png = 0;
        }
        in->setpos(start + i - (brute ? 255 : 31));
        detd = streamLength;
        return ZLIB;
      }
    }
    if( zh == -1 && zbuf[(zbufpos - 32) & 0xFF] == 'P' && zbuf[(zbufpos - 32 + 1) & 0xFF] == 'K' &&
        zbuf[(zbufpos - 32 + 2) & 0xFF] == '\x3' && zbuf[(zbufpos - 32 + 3) & 0xFF] == '\x4' && zbuf[(zbufpos - 32 + 8) & 0xFF] == '\x8' &&
        zbuf[(zbufpos - 32 + 9) & 0xFF] == '\0' ) {
      int nlen = (int) zbuf[(zbufpos - 32 + 26) & 0xFF] + ((int) zbuf[(zbufpos - 32 + 27) & 0xFF]) * 256 +
                 (int) zbuf[(zbufpos - 32 + 28) & 0xFF] + ((int) zbuf[(zbufpos - 32 + 29) & 0xFF]) * 256;
      if( nlen < 256 && i + 30 + nlen < n )
        zzippos = i + 30 + nlen;
    }
#endif //USE_ZLIB

    if( i - pdfimp > 1024 )
      pdfim = pdfimw = pdfimh = pdfimb = pdfgray = 0; // fail
    if( pdfim > 1 && !(isspace(c) || isdigit(c)))
      pdfim = 1;
    if( pdfim == 2 && isdigit(c))
      pdfimw = pdfimw * 10 + (c - '0');
    if( pdfim == 3 && isdigit(c))
      pdfimh = pdfimh * 10 + (c - '0');
    if( pdfim == 4 && isdigit(c))
      pdfimb = pdfimb * 10 + (c - '0');
    if((buf0 & 0xffff) == 0x3c3c )
      pdfimp = i, pdfim = 1; // <<
    if( pdfim && (buf1 & 0xffff) == 0x2f57 && buf0 == 0x69647468 )
      pdfim = 2, pdfimw = 0; // /Width
    if( pdfim && (buf1 & 0xffffff) == 0x2f4865 && buf0 == 0x69676874 )
      pdfim = 3, pdfimh = 0; // /Height
    if( pdfim && buf3 == 0x42697473 && buf2 == 0x50657243 && buf1 == 0x6f6d706f && buf0 == 0x6e656e74 &&
        zbuf[(zbufpos - 32 + 15) & 0xFF] == '/' )
      pdfim = 4, pdfimb = 0; // /BitsPerComponent
    if( pdfim && (buf2 & 0xFFFFFF) == 0x2F4465 && buf1 == 0x76696365 && buf0 == 0x47726179 )
      pdfgray = 1; // /DeviceGray

    // CD sectors detection (mode 1 and mode 2 form 1+2 - 2352 bytes)
    if( buf1 == 0x00ffffff && buf0 == 0xffffffff && !cdi )
      cdi = i, cda = -1, cdm = 0;
    if( cdi && i > cdi ) {
      const int p = (i - cdi) % 2352;
      if( p == 8 && (buf1 != 0xffffff00 || ((buf0 & 0xff) != 1 && (buf0 & 0xff) != 2)))
        cdi = 0;
      else if( p == 16 && i + 2336 < n ) {
        uint8_t data[2352];
        const uint64_t savedpos = in->curPos();
        in->setpos(start + i - 23);
        in->blockRead(data, 2352);
        in->setpos(savedpos);
        int t = expand_cd_sector(data, cda, 1);
        if( t != cdm )
          cdm = t * (i - cdi < 2352);
        if( cdm && cda != 10 && (cdm == 1 || buf0 == buf1)) {
          if( type != CD )
            return info = cdm, in->setpos(start + cdi - 7), CD;
          cda = (data[12] << 16) + (data[13] << 8) + data[14];
          if( cdm != 1 && i - cdi > 2352 && buf0 != cdf )
            cda = 10;
          if( cdm != 1 )
            cdf = buf0;
        } else
          cdi = 0;
      }
      if( !cdi && type == CD ) {
        in->setpos(start + i - p - 7);
        return DEFAULT;
      }
    }
    if( type == CD )
      continue;

    // Detect JPEG by code SOI APPx (FF D8 FF Ex) followed by
    // SOF0 (FF C0 xx xx 08) and SOS (FF DA) within a reasonable distance.
    // Detect end by any code other than RST0-RST7 (FF D9-D7) or
    // a byte stuff (FF 00).

    if( !soi && i >= 3 && (buf0 & 0xffffff00) == 0xffd8ff00 &&
        ((buf0 & 0xFE) == 0xC0 || (uint8_t) buf0 == 0xC4 || ((uint8_t) buf0 >= 0xDB && (uint8_t) buf0 <= 0xFE)))
      soi = i, app = i + 2, sos = sof = 0;
    if( soi ) {
      if( app == i && (buf0 >> 24) == 0xff && ((buf0 >> 16) & 0xff) > 0xc1 && ((buf0 >> 16) & 0xff) < 0xff )
        app = i + (buf0 & 0xffff) + 2;
      if( app < i && (buf1 & 0xff) == 0xff && (buf0 & 0xfe0000ff) == 0xc0000008 )
        sof = i;
      if( sof && sof > soi && i - sof < 0x1000 && (buf0 & 0xffff) == 0xffda ) {
        sos = i;
        if( type != JPEG )
          return in->setpos(start + soi - 3), JPEG;
      }
      if( i - soi > 0x40000 && !sos )
        soi = 0;
    }
    if( type == JPEG && sos && i > sos && (buf0 & 0xff00) == 0xff00 && (buf0 & 0xff) != 0 && (buf0 & 0xf8) != 0xd0 )
      return DEFAULT;
#ifdef USE_AUDIOMODEL
    // Detect .wav file header
    if( buf0 == 0x52494646 )
      wavi = i, wavm = wavlen = 0; //"RIFF"
    if( wavi ) {
      int p = i - wavi;
      if( p == 4 )
        wavsize = bswap(buf0); //fileSize
      else if( p == 8 ) {
        wavtype = (buf0 == 0x57415645 /*WAVE*/) ? 1 : (buf0 == 0x7366626B /*sfbk*/) ? 2 : 0;
        if( !wavtype )
          wavi = 0;
      } else if( wavtype ) {
        if( wavtype == 1 ) {
          if( p == 16 + wavlen && (buf1 != 0x666d7420 /*"fmt "*/ || ((wavm = bswap(buf0) - 16) & 0xFFFFFFFD) != 0))
            wavlen = ((bswap(buf0) + 1) & (-2)) + 8, wavi *= (buf1 == 0x666d7420 /*"fmt "*/ && (wavm & 0xFFFFFFFD) != 0);
          else if( p == 22 + wavlen )
            wavch = bswap(buf0) & 0xffff; // number of channels: 1 or 2
          else if( p == 34 + wavlen )
            wavbps = bswap(buf0) & 0xffff; // bits per smaple: 8 or 16
          else if( p == 40 + wavlen + wavm && buf1 != 0x64617461 /*"data"*/)
            wavm += ((bswap(buf0) + 1) & (-2)) + 8, wavi = (wavm > 0xfffff ? 0 : wavi);
          else if( p == 40 + wavlen + wavm ) {
            int wavd = bswap(buf0); // size of data section
            wavlen = 0;
            if((wavch == 1 || wavch == 2) && (wavbps == 8 || wavbps == 16) && wavd > 0 && wavsize >= wavd + 36 &&
               wavd % ((wavbps / 8) * wavch) == 0 )
              AUD_DET((wavbps == 8) ? AUDIO : AUDIO_LE, wavi - 3, 44 + wavm, wavd, wavch + wavbps / 4 - 3);
            wavi = 0;
          }
        } else {
          if((p == 16 && buf1 != 0x4C495354 /*LIST*/) || (p == 20 && buf0 != 0x494E464F /*INFO*/))
            wavi = 0;
          else if( p > 20 && buf1 == 0x4C495354 /*LIST*/ && (wavi *= (buf0 != 0))) {
            wavlen = bswap(buf0);
            wavlist = i;
          } else if( wavlist ) {
            p = i - wavlist;
            if( p == 8 && (buf1 != 0x73647461 /*sdta*/ || buf0 != 0x736D706C /*smpl*/))
              wavi = 0;
            else if( p == 12 ) {
              int wavd = bswap(buf0);
              if( wavd && (wavd + 12) == wavlen )
                AUD_DET(AUDIO_LE, wavi - 3, (12 + wavlist - (wavi - 3) + 1) & ~1, wavd, 1 + 16 / 4 - 3 /*mono, 16-bit*/);
              wavi = 0;
            }
          }
        }
      }
    }

    // Detect .aiff file header
    if( buf0 == 0x464f524d )
      aiff = i, aiffs = 0; // FORM
    if( aiff ) {
      const int p = i - aiff;
      if( p == 12 && (buf1 != 0x41494646 || buf0 != 0x434f4d4d))
        aiff = 0; // AIFF COMM
      else if( p == 24 ) {
        const int bits = buf0 & 0xffff, chn = buf1 >> 16;
        if((bits == 8 || bits == 16) && (chn == 1 || chn == 2))
          aiffm = chn + bits / 4 - 3 + 4;
        else
          aiff = 0;
      } else if( p == 42 + aiffs && buf1 != 0x53534e44 )
        aiffs += (buf0 + 8) + (buf0 & 1), aiff = (aiffs > 0x400 ? 0 : aiff);
      else if( p == 42 + aiffs )
        AUD_DET(AUDIO, aiff - 3, 54 + aiffs, buf0 - 8, aiffm);
    }

    // Detect .mod file header
    if((buf0 == 0x4d2e4b2e || buf0 == 0x3643484e || buf0 == 0x3843484e // M.K. 6CHN 8CHN
        || buf0 == 0x464c5434 || buf0 == 0x464c5438) && (buf1 & 0xc0c0c0c0) == 0 && i >= 1083 ) {
      const uint64_t savedpos = in->curPos();
      const int chn = ((buf0 >> 24) == 0x36 ? 6 : (((buf0 >> 24) == 0x38 || (buf0 & 0xff) == 0x38) ? 8 : 4));
      int len = 0; // total length of samples
      int numpat = 1; // number of patterns
      for( int j = 0; j < 31; j++ ) {
        in->setpos(start + i - 1083 + 42 + j * 30);
        const int i1 = in->getchar();
        const int i2 = in->getchar();
        len += i1 * 512 + i2 * 2;
      }
      in->setpos(start + i - 131);
      for( int j = 0; j < 128; j++ ) {
        int x = in->getchar();
        if( x + 1 > numpat )
          numpat = x + 1;
      }
      if( numpat < 65 )
        AUD_DET(AUDIO, i - 1083, 1084 + numpat * 256 * chn, len, 4 /*mono, 8-bit*/);
      in->setpos(savedpos);
    }

    // Detect .s3m file header
    if( buf0 == 0x1a100000 )
      s3mi = i, s3mno = s3mni = 0; //0x1A: signature byte, 0x10: song type, 0x0000: reserved
    if( s3mi ) {
      const int p = i - s3mi;
      if( p == 4 ) {
        s3mno = bswap(buf0) & 0xffff; //Number of entries in the order table, should be even
        s3mni = (bswap(buf0) >> 16); //Number of instruments in the song
      } else if( p == 16 && (((buf1 >> 16) & 0xff) != 0x13 || buf0 != 0x5343524d /*SCRM*/))
        s3mi = 0;
      else if( p == 16 ) {
        const uint64_t savedpos = in->curPos();
        int b[31], samStart = (1 << 16), samEnd = 0, ok = 1;
        for( int j = 0; j < s3mni; j++ ) {
          in->setpos(start + s3mi - 31 + 0x60 + s3mno + j * 2);
          int i1 = in->getchar();
          i1 += in->getchar() * 256;
          in->setpos(start + s3mi - 31 + i1 * 16);
          i1 = in->getchar();
          if( i1 == 1 ) { // type: sample
            for( int k = 0; k < 31; k++ )
              b[k] = in->getchar();
            int len = b[15] + (b[16] << 8);
            int ofs = b[13] + (b[14] << 8);
            if( b[30] > 1 )
              ok = 0;
            if( ofs * 16 < samStart )
              samStart = ofs * 16;
            if( ofs * 16 + len > samEnd )
              samEnd = ofs * 16 + len;
          }
        }
        if( ok && samStart < (1 << 16))
          AUD_DET(AUDIO, s3mi - 31, samStart, samEnd - samStart, 0 /*mono, 8-bit*/);
        s3mi = 0;
        in->setpos(savedpos);
      }
    }
#endif //  USE_AUDIOMODEL

    // Detect .bmp image
    if( !bmpi && !dibi ) {
      if((buf0 & 0xffff) == 16973 ) { // 'BM'
        bmpi = i; // header start: bmpi-1
        dibi = i - 1 + 18; // we expect a DIB header to come
      } else if( buf0 == 0x28000000 ) // headerless (DIB-only)
        dibi = i + 1;
    } else {
      const uint32_t p = i - dibi + 1 + 18;
      if( p == 10 + 4 )
        bmpof = bswap(buf0), bmpi = (bmpof < 54 || start + blockSize < bmpi - 1 + bmpof) ? (dibi = 0)
                                                                                         : bmpi; //offset of pixel data (this field is still located in the BMP Header)
      else if( p == 14 + 4 && buf0 != 0x28000000 )
        bmpi = dibi = 0; //BITMAPINFOHEADER (0x28)
      else if( p == 18 + 4 )
        bmpx = bswap(buf0), bmpi = (((bmpx & 0xff000000) != 0 || bmpx == 0) ? (dibi = 0) : bmpi); //width
      else if( p == 22 + 4 )
        bmpy = abs(int(bswap(buf0))), bmpi = (((bmpy & 0xff000000) != 0 || bmpy == 0) ? (dibi = 0) : bmpi); //height
      else if( p == 26 + 2 )
        bmpi = ((bswap(buf0 << 16)) != 1) ? (dibi = 0) : bmpi; //number of color planes (must be 1)
      else if( p == 28 + 2 )
        imgbpp = bswap(buf0 << 16), bmpi = ((imgbpp != 1 && imgbpp != 4 && imgbpp != 8 && imgbpp != 24 && imgbpp != 32) ? (dibi = 0)
                                                                                                                        : bmpi); //color depth
      else if( p == 30 + 4 )
        bmpi = ((buf0 != 0) ? (dibi = 0) : bmpi); //compression method must be: BI_RGB (uncompressed)
      else if( p == 34 + 4 )
        bmps = bswap(buf0); //image size or 0
        //else if (p==38+4) ; // the horizontal resolution of the image (ignored)
        //else if (p==42+4) ; // the vertical resolution of the image (ignored)
      else if( p == 46 + 4 ) {
        n_colors = bswap(buf0); // the number of colors in the color palette, or 0 to default to (1<<imgbpp)
        if( n_colors == 0 && imgbpp <= 8 )
          n_colors = 1 << imgbpp;
        if( n_colors > (1u << imgbpp) || (imgbpp > 8 && n_colors > 0))
          bmpi = dibi = 0;
      } else if( p == 50 + 4 ) { //the number of important colors used
        if( bswap(buf0) <= n_colors || bswap(buf0) == 0x10000000 ) {
          if( bmpi == 0 /*headerless*/ && (bmpx * 2 == bmpy) && imgbpp > 1 && // possible icon/cursor?
              ((bmps > 0 && bmps == ((bmpx * bmpy * (imgbpp + 1)) >> 4)) || ((!bmps || bmps < ((bmpx * bmpy * imgbpp) >> 3)) &&
                                                                             ((bmpx == 8) || (bmpx == 10) || (bmpx == 14) || (bmpx == 16) ||
                                                                              (bmpx == 20) || (bmpx == 22) || (bmpx == 24) ||
                                                                              (bmpx == 32) || (bmpx == 40) || (bmpx == 48) ||
                                                                              (bmpx == 60) || (bmpx == 64) || (bmpx == 72) ||
                                                                              (bmpx == 80) || (bmpx == 96) || (bmpx == 128) ||
                                                                              (bmpx == 256)))))
            bmpy = bmpx;

          BlockType blockType = DEFAULT;
          uint32_t widthInBytes = 0;
          if( imgbpp == 1 ) {
            blockType = IMAGE1;
            widthInBytes = (((bmpx - 1) >> 5) + 1) * 4;
          } else if( imgbpp == 4 ) {
            blockType = IMAGE4;
            widthInBytes = ((bmpx * 4 + 31) >> 5) * 4;
          } else if( imgbpp == 8 ) {
            blockType = IMAGE8;
            widthInBytes = (bmpx + 3) & -4;
          } else if( imgbpp == 24 ) {
            blockType = IMAGE24;
            widthInBytes = ((bmpx * 3) + 3) & -4;
          } else if( imgbpp == 32 ) {
            blockType = IMAGE32;
            widthInBytes = bmpx * 4;
          }

          if( imgbpp == 8 ) {
            const uint64_t colorPalettePos = dibi - 18 + 54;
            const uint64_t savedpos = in->curPos();
            in->setpos(colorPalettePos);
            if( IsGrayscalePalette(in, n_colors, 1))
              blockType = IMAGE8GRAY;
            in->setpos(savedpos);
          }

          const uint32_t headerpos = bmpi > 0 ? bmpi - 1 : dibi - 4;
          const uint32_t minheadersize = (bmpi > 0 ? 54 : 54 - 14) + n_colors * 4;
          const uint32_t headersize = bmpi > 0 ? bmpof : minheadersize;

          // some final sanity checks
          if( bmps != 0 &&
              bmps < widthInBytes * bmpy ) { /*printf("\nBMP guard: image is larger than reported in header\n",bmps,widthInBytes*bmpy);*/
          } else if( start + blockSize < headerpos + headersize + widthInBytes * bmpy ) { /*printf("\nBMP guard: cropped data\n");*/
          } else if( headersize == (bmpi > 0 ? 54 : 54 - 14) && n_colors > 0 ) { /*printf("\nBMP guard: missing palette\n");*/
          } else if( bmpi > 0 && bmpof < minheadersize ) { /*printf("\nBMP guard: overlapping color palette\n");*/
          } else if( bmpi > 0 && uint64_t(bmpi) - 1 + bmpof + widthInBytes * bmpy >
                                 start + blockSize ) { /*printf("\nBMP guard: reported pixel data offset is incorrect\n");*/
          } else if( widthInBytes * bmpy <= 64 ) { /*printf("\nBMP guard: too small\n");*/
          } // too small - not worthy to use the image models
          else
            IMG_DET(blockType, headerpos, headersize, widthInBytes, bmpy);
        }
        bmpi = dibi = 0;
      }
    }

    // Detect binary .pbm .pgm .ppm .pam images
    if((buf0 & 0xfff0ff) == 0x50300a ) { //"Px" + line feed, where "x" shall be a number
      pgmn = (buf0 & 0xf00) >> 8; // extract "x"
      if((pgmn >= 4 && pgmn <= 6) || pgmn == 7 )
        pgm = i, pgm_ptr = pgmw = pgmh = pgmc = pgmcomment = pamatr = pamd = pgmdata = pgmdatasize = 0; // "P4" (pbm), "P5" (pgm), "P6" (ppm), "P7" (pam)
    }
    if( pgm ) {
      if( pgmdata == 0 ) { // parse header
        if( i - pgm == 1 && c == 0x23 )
          pgmcomment = 1; // # (pgm comment)
        if( !pgmcomment && pgm_ptr ) {
          int s = 0;
          if( pgmn == 7 ) {
            if((buf1 & 0xdfdf) == 0x5749 && (buf0 & 0xdfdfdfff) == 0x44544820 )
              pgm_ptr = 0, pamatr = 1; // WIDTH
            if((buf1 & 0xdfdfdf) == 0x484549 && (buf0 & 0xdfdfdfff) == 0x47485420 )
              pgm_ptr = 0, pamatr = 2; // HEIGHT
            if((buf1 & 0xdfdfdf) == 0x4d4158 && (buf0 & 0xdfdfdfff) == 0x56414c20 )
              pgm_ptr = 0, pamatr = 3; // MAXVAL
            if((buf1 & 0xdfdf) == 0x4445 && (buf0 & 0xdfdfdfff) == 0x50544820 )
              pgm_ptr = 0, pamatr = 4; // DEPTH
            if((buf2 & 0xdf) == 0x54 && (buf1 & 0xdfdfdfdf) == 0x55504c54 && (buf0 & 0xdfdfdfff) == 0x59504520 )
              pgm_ptr = 0, pamatr = 5; // TUPLTYPE
            if((buf1 & 0xdfdfdf) == 0x454e44 && (buf0 & 0xdfdfdfff) == 0x4844520a )
              pgm_ptr = 0, pamatr = 6; // ENDHDR
            if( c == 0x0a ) {
              if( pamatr == 0 )
                pgm = 0;
              else if( pamatr < 5 )
                s = pamatr;
              if( pamatr != 6 )
                pamatr = 0;
            }
          } else if( c == 0x20 && !pgmw )
            s = 1;
          else if( c == 0x0a && !pgmh )
            s = 2;
          else if( c == 0x0a && !pgmc && pgmn != 4 )
            s = 3;
          if( s ) {
            pgm_buf[pgm_ptr++] = 0;
            int v = atoi(pgm_buf); // parse width/height/depth/maxval value
            if( s == 1 )
              pgmw = v;
            else if( s == 2 )
              pgmh = v;
            else if( s == 3 )
              pgmc = v;
            else if( s == 4 )
              pamd = v;
            if( v == 0 || (s == 3 && v > 255))
              pgm = 0;
            else
              pgm_ptr = 0;
          }
        }
        if( !pgmcomment )
          pgm_buf[pgm_ptr++] = c;
        if( pgm_ptr >= 32 )
          pgm = 0;
        if( pgmcomment && c == 0x0a )
          pgmcomment = 0;
        if( pgmw && pgmh && !pgmc && pgmn == 4 ) {
          pgmdata = i;
          pgmdatasize = (pgmw + 7) / 8 * pgmh;
        }
        if( pgmw && pgmh && pgmc && (pgmn == 5 || (pgmn == 7 && pamd == 1 && pamatr == 6))) {
          pgmdata = i;
          pgmdatasize = pgmw * pgmh;
        }
        if( pgmw && pgmh && pgmc && (pgmn == 6 || (pgmn == 7 && pamd == 3 && pamatr == 6))) {
          pgmdata = i;
          pgmdatasize = pgmw * 3 * pgmh;
        }
        if( pgmw && pgmh && pgmc && (pgmn == 7 && pamd == 4 && pamatr == 6)) {
          pgmdata = i;
          pgmdatasize = pgmw * 4 * pgmh;
        }
      } else { // pixel data
        if( textparser.start() == uint32_t(i) || // for any sign of non-text data in pixel area
            (pgm - 2 == 0 && n - pgmdatasize == i)) // or the image is the whole file/block -> FINISH (success)
        {
          if( pgmw && pgmh && !pgmc && pgmn == 4 )
            IMG_DET(IMAGE1, pgm - 2, pgmdata - pgm + 3, (pgmw + 7) / 8, pgmh);
          if( pgmw && pgmh && pgmc && (pgmn == 5 || (pgmn == 7 && pamd == 1 && pamatr == 6)))
            IMG_DET(IMAGE8GRAY, pgm - 2, pgmdata - pgm + 3, pgmw, pgmh);
          if( pgmw && pgmh && pgmc && (pgmn == 6 || (pgmn == 7 && pamd == 3 && pamatr == 6)))
            IMG_DET(IMAGE24, pgm - 2, pgmdata - pgm + 3, pgmw * 3, pgmh);
          if( pgmw && pgmh && pgmc && (pgmn == 7 && pamd == 4 && pamatr == 6))
            IMG_DET(IMAGE32, pgm - 2, pgmdata - pgm + 3, pgmw * 4, pgmh);
        } else if((--pgmdatasize) == 0 )
          pgm = 0; // all data was probably text in pixel area: fail
      }
    }

    // Detect .rgb image
    if((buf0 & 0xffff) == 0x01da )
      rgbi = i, rgbx = rgby = 0;
    if( rgbi ) {
      const int p = i - rgbi;
      if( p == 1 && c != 0 )
        rgbi = 0;
      else if( p == 2 && c != 1 )
        rgbi = 0;
      else if( p == 4 && (buf0 & 0xffff) != 1 && (buf0 & 0xffff) != 2 && (buf0 & 0xffff) != 3 )
        rgbi = 0;
      else if( p == 6 )
        rgbx = buf0 & 0xffff, rgbi = (rgbx == 0 ? 0 : rgbi);
      else if( p == 8 )
        rgby = buf0 & 0xffff, rgbi = (rgby == 0 ? 0 : rgbi);
      else if( p == 10 ) {
        int z = buf0 & 0xffff;
        if( rgbx && rgby && (z == 1 || z == 3 || z == 4))
          IMG_DET(IMAGE8, rgbi - 1, 512, rgbx, rgby * z);
        rgbi = 0;
      }
    }

    // Detect .tiff file header (2/8/24 bit color, not compressed).
    if( buf1 == 0x49492a00 && n > i + (int) bswap(buf0)) {
      const uint64_t savedpos = in->curPos();
      in->setpos(start + i + (uint64_t) bswap(buf0) - 7);

      // read directory
      int dirsize = in->getchar();
      int tifx = 0, tify = 0, tifz = 0, tifzb = 0, tifc = 0, tifofs = 0, tifofval = 0, tifsize = 0, b[12];
      if( in->getchar() == 0 ) {
        for( int i = 0; i < dirsize; i++ ) {
          for( int j = 0; j < 12; j++ )
            b[j] = in->getchar();
          if( b[11] == EOF)
            break;
          int tag = b[0] + (b[1] << 8);
          int tagfmt = b[2] + (b[3] << 8);
          int taglen = b[4] + (b[5] << 8) + (b[6] << 16) + (b[7] << 24);
          int tagval = b[8] + (b[9] << 8) + (b[10] << 16) + (b[11] << 24);
          if( tagfmt == 3 || tagfmt == 4 ) {
            if( tag == 256 )
              tifx = tagval;
            else if( tag == 257 )
              tify = tagval;
            else if( tag == 258 )
              tifzb = taglen == 1 ? tagval : 8; // bits per component
            else if( tag == 259 )
              tifc = tagval; // 1 = no compression
            else if( tag == 273 && tagfmt == 4 )
              tifofs = tagval, tifofval = (taglen <= 1);
            else if( tag == 277 )
              tifz = tagval; // components per pixel
            else if( tag == 279 && taglen == 1 )
              tifsize = tagval;
          }
        }
      }
      if( tifx && tify && tifzb && (tifz == 1 || tifz == 3) && ((tifc == 1) || (tifc == 5 /*LZW*/ && tifsize > 0)) &&
          (tifofs && tifofs + i < n)) {
        if( !tifofval ) {
          in->setpos(start + i + tifofs - 7);
          for( int j = 0; j < 4; j++ )
            b[j] = in->getchar();
          tifofs = b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
        }
        if( tifofs && tifofs < (1 << 18) && tifofs + i < n ) {
          if( tifc == 1 ) {
            if( tifz == 1 && tifzb == 1 )
              IMG_DET(IMAGE1, i - 7, tifofs, ((tifx - 1) >> 3) + 1, tify);
            else if( tifz == 1 && tifzb == 8 )
              IMG_DET(IMAGE8, i - 7, tifofs, tifx, tify);
            else if( tifz == 3 && tifzb == 8 )
              IMG_DET(IMAGE24, i - 7, tifofs, tifx * 3, tify);
          } else if( tifc == 5 && tifsize > 0 ) {
            tifx = ((tifx + 8 - tifzb) / (9 - tifzb)) * tifz;
            info = tifz * tifzb;
            info = (((info == 1) ? IMAGE1 : ((info == 8) ? IMAGE8 : IMAGE24)) << 24) | tifx;
            detd = tifsize;
            in->setpos(start + i - 7 + tifofs);
            return dett = LZW;
          }
        }
      }
      in->setpos(savedpos);
    }

    // Detect .tga image (8-bit 256 colors or 24-bit uncompressed)
    if((buf1 & 0xFFF7FF) == 0x00010100 && (buf0 & 0xFFFFFFC7) == 0x00000100 && (c == 16 || c == 24 || c == 32))
      tga = i, tgax = tgay, tgaz = 8, tgat = (buf1 >> 8) & 0xF, tgaid = buf1 >> 24, tgamap = c / 8;
    else if((buf1 & 0xFFFFFF) == 0x00000200 && buf0 == 0x00000000 )
      tga = i, tgax = tgay, tgaz = 24, tgat = 2;
    else if((buf1 & 0xFFF7FF) == 0x00000300 && buf0 == 0x00000000 )
      tga = i, tgax = tgay, tgaz = 8, tgat = (buf1 >> 8) & 0xF;
    if( tga ) {
      if( i - tga == 8 )
        tga = (buf1 == 0 ? tga : 0), tgax = (bswap(buf0) & 0xffff), tgay = (bswap(buf0) >> 16);
      else if( i - tga == 10 ) {
        if((buf0 & 0xFFF7) == 32 << 8 )
          tgaz = 32;
        if((tgaz << 8) == (int) (buf0 & 0xFFD7) && tgax && tgay && uint32_t(tgax * tgay) < 0xFFFFFFF ) {
          if( tgat == 1 ) {
            in->setpos(start + tga + 11 + tgaid);
            IMG_DET((IsGrayscalePalette(in)) ? IMAGE8GRAY : IMAGE8, tga - 7, 18 + tgaid + 256 * tgamap, tgax, tgay);
          } else if( tgat == 2 )
            IMG_DET((tgaz == 24) ? IMAGE24 : IMAGE32, tga - 7, 18 + tgaid, tgax * (tgaz >> 3), tgay);
          else if( tgat == 3 )
            IMG_DET(IMAGE8GRAY, tga - 7, 18 + tgaid, tgax, tgay);
          else if( tgat == 9 || tgat == 11 ) {
            const uint64_t savedpos = in->curPos();
            in->setpos(start + tga + 11 + tgaid);
            if( tgat == 9 ) {
              info = (IsGrayscalePalette(in) ? IMAGE8GRAY : IMAGE8) << 24;
              in->setpos(start + tga + 11 + tgaid + 256 * tgamap);
            } else
              info = IMAGE8GRAY << 24;
            info |= tgax;
            // now detect compressed image data size
            detd = 0;
            int c = in->getchar(), b = 0, total = tgax * tgay, line = 0;
            while( total > 0 && c >= 0 && (++detd, b = in->getchar()) >= 0 ) {
              if( c == 0x80 ) {
                c = b;
                continue;
              } else if( c > 0x7F ) {
                total -= (c = (c & 0x7F) + 1);
                line += c;
                c = in->getchar();
                detd++;
              } else {
                in->setpos(in->curPos() + c);
                detd += ++c;
                total -= c;
                line += c;
                c = in->getchar();
              }
              if( line > tgax )
                break;
              else if( line == tgax )
                line = 0;
            }
            if( total == 0 ) {
              in->setpos(start + tga + 11 + tgaid + 256 * tgamap);
              return dett = RLE;
            } else
              in->setpos(savedpos);
          }
        }
        tga = 0;
      }
    }

    // Detect .gif
    if( type == DEFAULT && dett == GIF && i == 0 ) {
      dett = DEFAULT;
      if( c == 0x2c || c == 0x21 )
        gif = 2, gifi = 2;
      else
        gifgray = 0;
    }
    if( !gif && (buf1 & 0xffff) == 0x4749 && (buf0 == 0x46383961 || buf0 == 0x46383761))
      gif = 1, gifi = i + 5;
    if( gif ) {
      if( gif == 1 && i == gifi )
        gif = 2, gifi = i + 5 + (gifplt = (c & 128) ? (3 * (2 << (c & 7))) : 0);
      if( gif == 2 && gifplt && i == gifi - gifplt - 3 )
        gifgray = IsGrayscalePalette(in, gifplt / 3), gifplt = 0;
      if( gif == 2 && i == gifi ) {
        if((buf0 & 0xff0000) == 0x210000 )
          gif = 5, gifi = i;
        else if((buf0 & 0xff0000) == 0x2c0000 )
          gif = 3, gifi = i;
        else
          gif = 0;
      }
      if( gif == 3 && i == gifi + 6 )
        gifw = (bswap(buf0) & 0xffff);
      if( gif == 3 && i == gifi + 7 )
        gif = 4, gifc = gifb = 0, gifa = gifi = i + 2 + (gifplt = ((c & 128) ? (3 * (2 << (c & 7))) : 0));
      if( gif == 4 && gifplt )
        gifgray = IsGrayscalePalette(in, gifplt / 3), gifplt = 0;
      if( gif == 4 && i == gifi ) {
        if( c > 0 && gifb && gifc != gifb )
          gifw = 0;
        if( c > 0 )
          gifb = gifc, gifc = c, gifi += c + 1;
        else if( !gifw )
          gif = 2, gifi = i + 3;
        else
          return in->setpos(start + gifa - 1), detd = i - gifa + 2, info = ((gifgray ? IMAGE8GRAY : IMAGE8) << 24) | gifw, dett = GIF;
      }
      if( gif == 5 && i == gifi ) {
        if( c > 0 )
          gifi += c + 1;
        else
          gif = 2, gifi = i + 3;
      }
    }

    // Detect EXE if the low order byte (little-endian) XX is more
    // recently seen (and within 4K) if a relative to absolute address
    // conversion is done in the context CALL/JMP (E8/E9) XX xx xx 00/FF
    // 4 times in a row.  Detect end of EXE at the last
    // place this happens when it does not happen for 64KB.

    if(((buf1 & 0xfe) == 0xe8 || (buf1 & 0xfff0) == 0x0f80) && ((buf0 + 1) & 0xfe) == 0 ) {
      int r = buf0 >> 24; // relative address low 8 bits
      int a = ((buf0 >> 24) + i) & 0xff; // absolute address low 8 bits
      int rdist = i - relpos[r];
      int adist = i - abspos[a];
      if( adist < rdist && adist < 0x800 && abspos[a] > 5 ) {
        e8e9last = i;
        ++e8e9count;
        if( e8e9pos == 0 || e8e9pos > abspos[a] )
          e8e9pos = abspos[a];
      } else
        e8e9count = 0;
      if( type == DEFAULT && e8e9count >= 4 && e8e9pos > 5 )
        return in->setpos(start + e8e9pos - 5), EXE;
      abspos[a] = i;
      relpos[r] = i;
    }
    if( i - e8e9last > 0x4000 ) {
      //TODO: Large file support
      if( type == EXE ) {
        info = (int) start;
        in->setpos(start + e8e9last);
        return DEFAULT;
      }
      e8e9count = e8e9pos = 0;
    }

    // Detect base64 encoded data
    if( b64s == 0 && buf0 == 0x73653634 && ((buf1 & 0xffffff) == 0x206261 || (buf1 & 0xffffff) == 0x204261))
      b64s = 1, b64i = i - 6; //' base64' ' Base64'
    if( b64s == 0 && ((buf1 == 0x3b626173 && buf0 == 0x6536342c) || (buf1 == 0x215b4344 && buf0 == 0x4154415b)))
      b64s = 3, b64i = i + 1; // ';base64,' '![CDATA['
    if( b64s > 0 ) {
      if( b64s == 1 && buf0 == 0x0d0a0d0a )
        b64i = i + 1, b64line = 0, b64s = 2;
      else if( b64s == 2 && (buf0 & 0xffff) == 0x0d0a && b64line == 0 )
        b64line = i + 1 - b64i, b64nl = i;
      else if( b64s == 2 && (buf0 & 0xffff) == 0x0d0a && b64line > 0 && (buf0 & 0xffffff) != 0x3d0d0a ) {
        if( i - b64nl < b64line && buf0 != 0x0d0a0d0a )
          i -= 1, b64s = 5;
        else if( buf0 == 0x0d0a0d0a )
          i -= 3, b64s = 5;
        else if( i - b64nl == b64line )
          b64nl = i;
        else
          b64s = 0;
      } else if( b64s == 2 && (buf0 & 0xffffff) == 0x3d0d0a )
        i -= 1, b64s = 5; // '=' or '=='
      else if( b64s == 2 && !(isalnum(c) || c == '+' || c == '/' || c == 10 || c == 13 || c == '='))
        b64s = 0;
      if( b64line > 0 && (b64line <= 4 || b64line > 255))
        b64s = 0;
      if( b64s == 3 && i >= b64i && !(isalnum(c) || c == '+' || c == '/' || c == '='))
        b64s = 4;
      if((b64s == 4 && i - b64i > 128) || (b64s == 5 && i - b64i > 512 && i - b64i < (1 << 27)))
        return in->setpos(start + b64i), detd = i - b64i, BASE64;
      if( b64s > 3 )
        b64s = 0;
      if( b64s == 1 && i - b64i >= 128 )
        b64s = 0; // detect false positives after 128 bytes
    }


    // Detect text
    // This is different from the above detection routines: it's a negative detection (it detects a failure)
    uint32_t t = utf8StateTable[c];
    textparser.UTF8State = utf8StateTable[256 + textparser.UTF8State + t];
    if( textparser.UTF8State == UTF8_ACCEPT ) { // proper end of a valid utf8 sequence
      if( c == NEW_LINE ) {
        if(((buf0 >> 8) & 0xff) != CARRIAGE_RETURN )
          textparser.setEOLType(2); // mixed or LF-only
        else if( textparser.EOLType() == 0 )
          textparser.setEOLType(1); // CRLF-only
      }
      textparser.invalidCount = textparser.invalidCount * (TEXT_ADAPT_RATE - 1) / TEXT_ADAPT_RATE;
      if( textparser.invalidCount == 0 )
        textparser.setend(i); // a possible end of block position
    } else if( textparser.UTF8State == UTF8_REJECT ) { // illegal state
      textparser.invalidCount = textparser.invalidCount * (TEXT_ADAPT_RATE - 1) / TEXT_ADAPT_RATE + TEXT_ADAPT_RATE;
      textparser.UTF8State = UTF8_ACCEPT; // reset state
      if( textparser.validlength() < TEXT_MIN_SIZE ) {
        textparser.reset(i + 1); // it's not text (or not long enough) - start over
      } else if( textparser.invalidCount >= TEXT_MAX_MISSES * TEXT_ADAPT_RATE ) {
        if( textparser.validlength() < TEXT_MIN_SIZE )
          textparser.reset(i + 1); // it's not text (or not long enough) - start over
        else // Commit text block validation
          textparser.next(i + 1);
      }
    }
  }
  return type;
}


typedef enum {
    FDECOMPRESS, FCOMPARE, FDISCARD
} FMode;

void encode_cd(File *in, File *out, uint64_t size, int info) {
  //TODO: Large file support
  const int BLOCK = 2352;
  uint8_t blk[BLOCK];
  uint64_t blockresidual = size % BLOCK;
  assert(blockresidual < 65536);
  out->putChar((blockresidual >> 8) & 255);
  out->putChar(blockresidual & 255);
  for( uint64_t offset = 0; offset < size; offset += BLOCK ) {
    if( offset + BLOCK > size ) { //residual
      in->blockRead(&blk[0], size - offset);
      out->blockWrite(&blk[0], size - offset);
    } else { //normal sector
      in->blockRead(&blk[0], BLOCK);
      if( info == 3 )
        blk[15] = 3; //indicate Mode2/Form2
      if( offset == 0 )
        out->blockWrite(&blk[12],
                        4 + 4 * (blk[15] != 1)); //4-byte address + 4 bytes from the 8-byte subheader goes only to the first sector
      out->blockWrite(&blk[16 + 8 * (blk[15] != 1)], 2048 + 276 * (info == 3)); //user data goes to all sectors
      if( offset + BLOCK * 2 > size && blk[15] != 1 )
        out->blockWrite(&blk[16], 4); //in Mode2 4 bytes from the 8-byte subheader goes after the last sector
    }
  }
}

uint64_t decode_cd(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  //TODO: Large file support
  const int BLOCK = 2352;
  uint8_t blk[BLOCK];
  uint64_t i = 0; //*in position
  uint64_t nextblockpos = 0;
  int address = -1;
  int datasize = 0;
  uint64_t residual = (in->getchar() << 8) + in->getchar();
  size -= 2;
  while( i < size ) {
    if( size - i == residual ) { //residual data after last sector
      in->blockRead(blk, residual);
      if( mode == FDECOMPRESS )
        out->blockWrite(blk, residual);
      else if( mode == FCOMPARE )
        for( int j = 0; j < (int) residual; ++j )
          if( blk[j] != out->getchar() && !diffFound )
            diffFound = nextblockpos + j + 1;
      return nextblockpos + residual;
    } else if( i == 0 ) { //first sector
      in->blockRead(blk + 12,
                    4); //header (4 bytes) consisting of address (Minutes, Seconds, Sectors) and mode (1 = Mode1, 2 = Mode2/Form1, 3 = Mode2/Form2)
      if( blk[15] != 1 )
        in->blockRead(blk + 16, 4); //Mode2: 4 bytes from the read 8-byte subheader
      datasize = 2048 + (blk[15] == 3) *
                        276; //user data bytes: Mode1 and Mode2/Form1: 2048 (ECC is present) or Mode2/Form2: 2048+276=2324 bytes (ECC is not present)
      i += 4 + 4 * (blk[15] != 1); //4 byte header + ( Mode2: 4 bytes from the 8-byte subheader )
    } else { //normal sector
      address = (blk[12] << 16) + (blk[13] << 8) + blk[14]; //3-byte address (Minutes, Seconds, Sectors)
    }
    in->blockRead(blk + 16 + (blk[15] != 1) * 8,
                  datasize); //read data bytes, but skip 8-byte subheader in Mode 2 (which we processed already above)
    i += datasize;
    if( datasize > 2048 )
      blk[15] = 3; //indicate Mode2/Form2
    if( blk[15] != 1 && size - residual - i == 4 ) { //Mode 2: we are at the last sector - grab the 4 subheader bytes
      in->blockRead(blk + 16, 4);
      i += 4;
    }
    expand_cd_sector(blk, address, 0);
    if( mode == FDECOMPRESS )
      out->blockWrite(blk, BLOCK);
    else if( mode == FCOMPARE )
      for( int j = 0; j < BLOCK; ++j )
        if( blk[j] != out->getchar() && !diffFound )
          diffFound = nextblockpos + j + 1;
    nextblockpos += BLOCK;
  }
  return nextblockpos;
}


// 24-bit image data transforms, controlled by OPTION_SKIPRGB:
// - simple color transform (b, g, r) -> (g, g-r, g-b)
// - channel reorder only (b, g, r) -> (g, r, b)
// Detects RGB565 to RGB888 conversions

#define RGB565_MIN_RUN 63

void encode_bmp(File *in, File *out, uint64_t len, int width) {
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
      out->putChar(options & OPTION_SKIPRGB ? r : g - r);
      out->putChar(options & OPTION_SKIPRGB ? b : g - b);
    }
    for( int j = 0; j < width % 3; j++ )
      out->putChar(in->getchar());
  }
  for( int i = len % width; i > 0; i-- )
    out->putChar(in->getchar());
}

uint64_t decode_bmp(Encoder &en, uint64_t size, int width, File *out, FMode mode, uint64_t &diffFound) {
  int r, g, b, p, total = 0;
  bool isPossibleRGB565 = true;
  for( int i = 0; i < (int) (size / width); i++ ) {
    p = i * width;
    for( int j = 0; j < width / 3; j++ ) {
      g = en.decompress();
      r = en.decompress();
      b = en.decompress();
      if( !(options & OPTION_SKIPRGB))
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

// 32-bit image
void encode_im32(File *in, File *out, uint64_t len, int width) {
  int r, g, b, a;
  for( int i = 0; i < (int) (len / width); i++ ) {
    for( int j = 0; j < width / 4; j++ ) {
      b = in->getchar();
      g = in->getchar();
      r = in->getchar();
      a = in->getchar();
      out->putChar(g);
      out->putChar(options & OPTION_SKIPRGB ? r : g - r);
      out->putChar(options & OPTION_SKIPRGB ? b : g - b);
      out->putChar(a);
    }
    for( int j = 0; j < width % 4; j++ )
      out->putChar(in->getchar());
  }
  for( int i = len % width; i > 0; i-- )
    out->putChar(in->getchar());
}

uint64_t decode_im32(Encoder &en, uint64_t size, int width, File *out, FMode mode, uint64_t &diffFound) {
  int r, g, b, a, p;
  bool rgb = (width & (1 << 31)) > 0;
  if( rgb )
    width ^= (1 << 31);
  for( int i = 0; i < (int) (size / width); i++ ) {
    p = i * width;
    for( int j = 0; j < width / 4; j++ ) {
      b = en.decompress(), g = en.decompress(), r = en.decompress(), a = en.decompress();
      if( mode == FDECOMPRESS ) {
        out->putChar(options & OPTION_SKIPRGB ? r : b - r);
        out->putChar(b);
        out->putChar(options & OPTION_SKIPRGB ? g : b - g);
        out->putChar(a);
        if( !j && !(i & 0xf))
          en.printStatus();
      } else if( mode == FCOMPARE ) {
        if(((options & OPTION_SKIPRGB ? r : b - r) & 255) != out->getchar() && !diffFound )
          diffFound = p + 1;
        if( b != out->getchar() && !diffFound )
          diffFound = p + 2;
        if(((options & OPTION_SKIPRGB ? g : b - g) & 255) != out->getchar() && !diffFound )
          diffFound = p + 3;
        if(((a) & 255) != out->getchar() && !diffFound )
          diffFound = p + 4;
        p += 4;
      }
    }
    for( int j = 0; j < width % 4; j++ ) {
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

void encode_endianness16b(File *in, File *out, uint64_t size) {
  for( uint64_t i = 0, l = size >> 1; i < l; i++ ) {
    uint8_t B = in->getchar();
    out->putChar(in->getchar());
    out->putChar(B);
  }
  if((size & 1) > 0 )
    out->putChar(in->getchar());
}

uint64_t decode_endianness16b(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  for( uint64_t i = 0, l = size >> 1; i < l; i++ ) {
    uint8_t b1 = en.decompress(), b2 = en.decompress();
    if( mode == FDECOMPRESS ) {
      out->putChar(b2);
      out->putChar(b1);
    } else if( mode == FCOMPARE ) {
      bool ok = out->getchar() == b2;
      ok &= out->getchar() == b1;
      if( !ok && !diffFound ) {
        diffFound = size - i * 2;
        break;
      }
    }
    if( mode == FDECOMPRESS && !(i & 0x7FF))
      en.printStatus();
  }
  if( !diffFound && (size & 1) > 0 ) {
    if( mode == FDECOMPRESS )
      out->putChar(en.decompress());
    else if( mode == FCOMPARE ) {
      if( out->getchar() != en.decompress())
        diffFound = size - 1;
    }
  }
  return size;
}

// EOL transform

void encode_eol(File *in, File *out, uint64_t len) {
  uint8_t b = 0, pB = 0;
  for( int i = 0; i < (int) len; i++ ) {
    b = in->getchar();
    if( pB == CARRIAGE_RETURN && b != NEW_LINE )
      out->putChar(pB);
    if( b != CARRIAGE_RETURN )
      out->putChar(b);
    pB = b;
  }
  if( b == CARRIAGE_RETURN )
    out->putChar(b);
}

uint64_t decode_eol(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t b;
  uint64_t count = 0;
  for( int i = 0; i < (int) size; i++, count++ ) {
    if((b = en.decompress()) == NEW_LINE ) {
      if( mode == FDECOMPRESS )
        out->putChar(CARRIAGE_RETURN);
      else if( mode == FCOMPARE ) {
        if( out->getchar() != CARRIAGE_RETURN && !diffFound ) {
          diffFound = size - i;
          break;
        }
      }
      count++;
    }
    if( mode == FDECOMPRESS )
      out->putChar(b);
    else if( mode == FCOMPARE ) {
      if( b != out->getchar() && !diffFound ) {
        diffFound = size - i;
        break;
      }
    }
    if( mode == FDECOMPRESS && !(i & 0xFFF))
      en.printStatus();
  }
  return count;
}

void encode_rle(File *in, File *out, uint64_t size, int info, int &hdrsize) {
  uint8_t b, c = in->getchar();
  int i = 1, maxBlockSize = info & 0xFFFFFF;
  out->putVLI(maxBlockSize);
  hdrsize = VLICost(maxBlockSize);
  while( i < (int) size ) {
    b = in->getchar(), i++;
    if( c == 0x80 ) {
      c = b;
      continue;
    } else if( c > 0x7F ) {
      for( int j = 0; j <= (c & 0x7F); j++ )
        out->putChar(b);
      c = in->getchar(), i++;
    } else {
      for( int j = 0; j <= c; j++, i++ ) {
        out->putChar(b), b = in->getchar();
      }
      c = b;
    }
  }
}

#define rleOutputRun \
  { \
    while (run > 128) { \
      *outPtr++ = 0xFF, *outPtr++ = byte; \
      run -= 128; \
    } \
    *outPtr++ = (uint8_t)(0x80 | (run - 1)), *outPtr++ = byte; \
  }

uint64_t decode_rle(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t inBuffer[0x10000] = {0};
  uint8_t outBuffer[0x10200] = {0};
  uint64_t pos = 0;
  int maxBlockSize = (int) in->getVLI();
  enum {
      BASE, LITERAL, RUN, LITERAL_RUN
  } state;
  do {
    uint64_t remaining = in->blockRead(&inBuffer[0], maxBlockSize);
    uint8_t *inPtr = (uint8_t *) inBuffer;
    uint8_t *outPtr = (uint8_t *) outBuffer;
    uint8_t *lastLiteral = nullptr;
    state = BASE;
    while( remaining > 0 ) {
      uint8_t byte = *inPtr++, loop = 0;
      int run = 1;
      for( remaining--; remaining > 0 && byte == *inPtr; remaining--, run++, inPtr++ );
      do {
        loop = 0;
        switch( state ) {
          case BASE:
          case RUN: {
            if( run > 1 ) {
              state = RUN;
              rleOutputRun
            } else {
              lastLiteral = outPtr;
              *outPtr++ = 0, *outPtr++ = byte;
              state = LITERAL;
            }
            break;
          }
          case LITERAL: {
            if( run > 1 ) {
              state = LITERAL_RUN;
              rleOutputRun
            } else {
              if( ++(*lastLiteral) == 127 )
                state = BASE;
              *outPtr++ = byte;
            }
            break;
          }
          case LITERAL_RUN: {
            if( outPtr[-2] == 0x81 && *lastLiteral < (125)) {
              state = (((*lastLiteral) += 2) == 127) ? BASE : LITERAL;
              outPtr[-2] = outPtr[-1];
            } else
              state = RUN;
            loop = 1;
          }
        }
      } while( loop );
    }

    uint64_t length = outPtr - (&outBuffer[0]);
    if( mode == FDECOMPRESS )
      out->blockWrite(&outBuffer[0], length);
    else if( mode == FCOMPARE ) {
      for( int j = 0; j < (int) length; ++j ) {
        if( outBuffer[j] != out->getchar() && !diffFound ) {
          diffFound = pos + j + 1;
          break;
        }
      }
    }
    pos += length;
  } while( !in->eof() && !diffFound );
  return pos;
}

struct LZWentry {
    short prefix;
    short suffix;
};

#define LZW_RESET_CODE 256
#define LZW_EOF_CODE 257

class LZWDictionary {
private:
    static constexpr int HashSize = 9221;
    LZWentry dictionary[4096];
    short table[HashSize];
    uint8_t buffer[4096];

public:
    int index;

    LZWDictionary() : index(0) { reset(); }

    void reset() {
      memset(&dictionary, 0xFF, sizeof(dictionary));
      memset(&table, 0xFF, sizeof(table));
      for( int i = 0; i < 256; i++ ) {
        table[-findEntry(-1, i) - 1] = (short) i;
        dictionary[i].suffix = i;
      }
      index = 258; //2 extra codes, one for resetting the dictionary and one for signaling EOF
    }

    int findEntry(const int prefix, const int suffix) {
      int i = finalize64(hash(prefix, suffix), 13);
      int offset = (i > 0) ? HashSize - i : 1;
      while( true ) {
        if( table[i] < 0 ) //free slot?
          return -i - 1;
        else if( dictionary[table[i]].prefix == prefix && dictionary[table[i]].suffix == suffix ) //is it the entry we want?
          return table[i];
        i -= offset;
        if( i < 0 )
          i += HashSize;
      }
    }

    void addEntry(const int prefix, const int suffix, const int offset = -1) {
      if( prefix == -1 || prefix >= index || index > 4095 || offset >= 0 )
        return;
      dictionary[index].prefix = prefix;
      dictionary[index].suffix = suffix;
      table[-offset - 1] = index;
      index += (index < 4096);
    }

    int dumpEntry(File *f, int code) {
      int n = 4095;
      while( code > 256 && n >= 0 ) {
        buffer[n] = uint8_t(dictionary[code].suffix);
        n--;
        code = dictionary[code].prefix;
      }
      buffer[n] = uint8_t(code);
      f->blockWrite(&buffer[n], 4096 - n);
      return code;
    }
};

int encode_lzw(File *in, File *out, uint64_t size, int &hdrsize) {
  LZWDictionary dic;
  int parent = -1, code = 0, buffer = 0, bitsPerCode = 9, bitsUsed = 0;
  bool done = false;
  while( !done ) {
    buffer = in->getchar();
    if( buffer < 0 ) {
      return 0;
    }
    for( int j = 0; j < 8; j++ ) {
      code += code + ((buffer >> (7 - j)) & 1), bitsUsed++;
      if( bitsUsed >= bitsPerCode ) {
        if( code == LZW_EOF_CODE ) {
          done = true;
          break;
        } else if( code == LZW_RESET_CODE ) {
          dic.reset();
          parent = -1;
          bitsPerCode = 9;
        } else {
          if( code < dic.index ) {
            if( parent != -1 )
              dic.addEntry(parent, dic.dumpEntry(out, code));
            else
              out->putChar(code);
          } else if( code == dic.index ) {
            int a = dic.dumpEntry(out, parent);
            out->putChar(a);
            dic.addEntry(parent, a);
          } else
            return 0;
          parent = code;
        }
        bitsUsed = 0;
        code = 0;
        if((1 << bitsPerCode) == dic.index + 1 && dic.index < 4096 )
          bitsPerCode++;
      }
    }
  }
  return 1;
}

inline void writeCode(File *f, const FMode mode, int *buffer, uint64_t *pos, int *bitsUsed, const int bitsPerCode, const int code,
                      uint64_t *diffFound) {
  *buffer <<= bitsPerCode;
  *buffer |= code;
  (*bitsUsed) += bitsPerCode;
  while((*bitsUsed) > 7 ) {
    const uint8_t b = *buffer >> (*bitsUsed -= 8);
    (*pos)++;
    if( mode == FDECOMPRESS )
      f->putChar(b);
    else if( mode == FCOMPARE && b != f->getchar())
      *diffFound = *pos;
  }
}

uint64_t decode_lzw(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  LZWDictionary dic;
  uint64_t pos = 0;
  int parent = -1, code = 0, buffer = 0, bitsPerCode = 9, bitsUsed = 0;
  writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_RESET_CODE, &diffFound);
  while((code = in->getchar()) >= 0 && diffFound == 0 ) {
    int index = dic.findEntry(parent, code);
    if( index < 0 ) { // entry not found
      writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, parent, &diffFound);
      if( dic.index > 4092 ) {
        writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_RESET_CODE, &diffFound);
        dic.reset();
        bitsPerCode = 9;
      } else {
        dic.addEntry(parent, code, index);
        if( dic.index >= (1 << bitsPerCode))
          bitsPerCode++;
      }
      parent = code;
    } else
      parent = index;
  }
  if( parent >= 0 )
    writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, parent, &diffFound);
  writeCode(out, mode, &buffer, &pos, &bitsUsed, bitsPerCode, LZW_EOF_CODE, &diffFound);
  if( bitsUsed > 0 ) { // flush buffer
    pos++;
    if( mode == FDECOMPRESS )
      out->putChar(uint8_t(buffer));
    else if( mode == FCOMPARE && uint8_t(buffer) != out->getchar())
      diffFound = pos;
  }
  return pos;
}

// EXE transform: <encoded-size> <begin> <block>...
// Encoded-size is 4 bytes, MSB first.
// begin is the offset of the start of the input file, 4 bytes, MSB first.
// Each block applies the e8e9 transform to strings falling entirely
// within the block starting from the end and working backwards.
// The 5 byte pattern is E8/E9 xx xx xx 00/FF (x86 CALL/JMP xxxxxxxx)
// where xxxxxxxx is a relative address LSB first.  The address is
// converted to an absolute address by adding the offset mod 2^25
// (in range +-2^24).

void encode_exe(File *in, File *out, uint64_t len, uint64_t begin) {
  const int BLOCK = 0x10000;
  Array<uint8_t> blk(BLOCK);
  out->putVLI(begin); //TODO: Large file support

  // Transform
  for( uint64_t offset = 0; offset < len; offset += BLOCK ) {
    uint32_t size = min(uint32_t(len - offset), BLOCK);
    int bytesRead = (int) in->blockRead(&blk[0], size);
    if( bytesRead != (int) size )
      quit("encode_exe read error");
    for( int i = bytesRead - 1; i >= 5; --i ) {
      if((blk[i - 4] == 0xe8 || blk[i - 4] == 0xe9 || (blk[i - 5] == 0x0f && (blk[i - 4] & 0xf0) == 0x80)) &&
         (blk[i] == 0 || blk[i] == 0xff)) {
        int a = (blk[i - 3] | blk[i - 2] << 8 | blk[i - 1] << 16 | blk[i] << 24) + (int) (offset + begin) + i + 1;
        a <<= 7;
        a >>= 7;
        blk[i] = a >> 24;
        blk[i - 1] = a ^ 176;
        blk[i - 2] = (a >> 8) ^ 176;
        blk[i - 3] = (a >> 16) ^ 176;
      }
    }
    out->blockWrite(&blk[0], bytesRead);
  }
}

uint64_t decode_exe(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  const int BLOCK = 0x10000; // block size
  int begin, offset = 6, a;
  uint8_t c[6];
  begin = (int) en.decodeBlockSize(); //TODO: Large file support
  size -= VLICost(uint64_t(begin));
  for( int i = 4; i >= 0; i-- )
    c[i] = en.decompress(); // Fill queue

  while( offset < (int) size + 6 ) {
    memmove(c + 1, c, 5);
    if( offset <= (int) size )
      c[0] = en.decompress();
    // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
    if((c[0] == 0x00 || c[0] == 0xFF) && (c[4] == 0xE8 || c[4] == 0xE9 || (c[5] == 0x0F && (c[4] & 0xF0) == 0x80)) &&
       (((offset - 1) ^ (offset - 6)) & -BLOCK) == 0 && offset <= (int) size ) { // not crossing block boundary
      a = ((c[1] ^ 176) | (c[2] ^ 176) << 8 | (c[3] ^ 176) << 16 | c[0] << 24) - offset - begin;
      a <<= 7;
      a >>= 7;
      c[3] = a;
      c[2] = a >> 8;
      c[1] = a >> 16;
      c[0] = a >> 24;
    }
    if( mode == FDECOMPRESS )
      out->putChar(c[5]);
    else if( mode == FCOMPARE && c[5] != out->getchar() && !diffFound )
      diffFound = offset - 6 + 1;
    if( mode == FDECOMPRESS && !(offset & 0xfff))
      en.printStatus();
    offset++;
  }
  return size;
}

#ifdef USE_ZLIB
MTFList MTF(81);

int encode_zlib(File *in, File *out, uint64_t len, int &hdrsize) {
  const int BLOCK = 1 << 16, LIMIT = 128;
  uint8_t zin[BLOCK * 2], zout[BLOCK], zrec[BLOCK * 2], diffByte[81 * LIMIT];
  uint64_t diffPos[81 * LIMIT];

  // Step 1 - parse offset type form zlib stream header
  uint64_t pos_backup = in->curPos();
  uint32_t h1 = in->getchar(), h2 = in->getchar();
  in->setpos(pos_backup);
  int zh = parse_zlib_header(h1 * 256 + h2);
  int memlevel, clevel, window = zh == -1 ? 0 : MAX_WBITS + 10 + zh / 4, ctype = zh % 4;
  int minclevel = window == 0 ? 1 : ctype == 3 ? 7 : ctype == 2 ? 6 : ctype == 1 ? 2 : 1;
  int maxclevel = window == 0 ? 9 : ctype == 3 ? 9 : ctype == 2 ? 6 : ctype == 1 ? 5 : 1;
  int index = -1, nTrials = 0;
  bool found = false;

  // Step 2 - check recompressiblitiy, determine parameters and save differences
  z_stream main_strm, rec_strm[81];
  int diffCount[81], recpos[81], main_ret = Z_STREAM_END;
  main_strm.zalloc = Z_NULL;
  main_strm.zfree = Z_NULL;
  main_strm.opaque = Z_NULL;
  main_strm.next_in = Z_NULL;
  main_strm.avail_in = 0;
  if( zlib_inflateInit(&main_strm, zh) != Z_OK )
    return false;
  for( int i = 0; i < 81; i++ ) {
    clevel = (i / 9) + 1;
    // Early skip if invalid parameter
    if( clevel < minclevel || clevel > maxclevel ) {
      diffCount[i] = LIMIT;
      continue;
    }
    memlevel = (i % 9) + 1;
    rec_strm[i].zalloc = Z_NULL;
    rec_strm[i].zfree = Z_NULL;
    rec_strm[i].opaque = Z_NULL;
    rec_strm[i].next_in = Z_NULL;
    rec_strm[i].avail_in = 0;
    int ret = deflateInit2(&rec_strm[i], clevel, Z_DEFLATED, window - MAX_WBITS, memlevel, Z_DEFAULT_STRATEGY);
    diffCount[i] = (ret == Z_OK) ? 0 : LIMIT;
    recpos[i] = BLOCK * 2;
    diffPos[i * LIMIT] = 0xFFFFFFFFFFFFFFFF;
    diffByte[i * LIMIT] = 0;
  }

  for( uint64_t i = 0; i < len; i += BLOCK ) {
    uint32_t blsize = min(uint32_t(len - i), BLOCK);
    nTrials = 0;
    for( int j = 0; j < 81; j++ ) {
      if( diffCount[j] == LIMIT )
        continue;
      nTrials++;
      if( recpos[j] >= BLOCK )
        recpos[j] -= BLOCK;
    }
    // early break if nothing left to test
    if( nTrials == 0 )
      break;
    memmove(&zrec[0], &zrec[BLOCK], BLOCK);
    memmove(&zin[0], &zin[BLOCK], BLOCK);
    in->blockRead(&zin[BLOCK], blsize); // Read block from input file

    // Decompress/inflate block
    main_strm.next_in = &zin[BLOCK];
    main_strm.avail_in = blsize;
    do {
      main_strm.next_out = &zout[0];
      main_strm.avail_out = BLOCK;
      main_ret = inflate(&main_strm, Z_FINISH);
      nTrials = 0;

      // Recompress/deflate block with all possible parameters
      for( int j = MTF.getFirst(); j >= 0; j = MTF.getNext()) {
        if( diffCount[j] == LIMIT )
          continue;
        nTrials++;
        rec_strm[j].next_in = &zout[0];
        rec_strm[j].avail_in = BLOCK - main_strm.avail_out;
        rec_strm[j].next_out = &zrec[recpos[j]];
        rec_strm[j].avail_out = BLOCK * 2 - recpos[j];
        int ret = deflate(&rec_strm[j], main_strm.total_in == len ? Z_FINISH : Z_NO_FLUSH);
        if( ret != Z_BUF_ERROR && ret != Z_STREAM_END && ret != Z_OK ) {
          diffCount[j] = LIMIT;
          continue;
        }

        // Compare
        int end = 2 * BLOCK - (int) rec_strm[j].avail_out;
        int tail = max(main_ret == Z_STREAM_END ? (int) len - (int) rec_strm[j].total_out : 0, 0);
        for( int k = recpos[j]; k < end + tail; k++ ) {
          if((k < end && i + k - BLOCK < len && zrec[k] != zin[k]) || k >= end ) {
            if( ++diffCount[j] < LIMIT ) {
              const int p = j * LIMIT + diffCount[j];
              diffPos[p] = i + k - BLOCK;
              assert(k < int(sizeof(zin) / sizeof(*zin)));
              diffByte[p] = zin[k];
            }
          }
        }
        // Early break on perfect match
        if( main_ret == Z_STREAM_END && diffCount[j] == 0 ) {
          index = j;
          found = true;
          break;
        }
        recpos[j] = 2 * BLOCK - rec_strm[j].avail_out;
      }
    } while( main_strm.avail_out == 0 && main_ret == Z_BUF_ERROR && nTrials > 0 );
    if((main_ret != Z_BUF_ERROR && main_ret != Z_STREAM_END) || nTrials == 0 )
      break;
  }
  int minCount = (found) ? 0 : LIMIT;
  for( int i = 80; i >= 0; i-- ) {
    clevel = (i / 9) + 1;
    if( clevel >= minclevel && clevel <= maxclevel )
      deflateEnd(&rec_strm[i]);
    if( !found && diffCount[i] < minCount )
      minCount = diffCount[index = i];
  }
  inflateEnd(&main_strm);
  if( minCount == LIMIT )
    return false;
  MTF.moveToFront(index);

  // Step 3 - write parameters, differences and precompressed (inflated) data
  out->putChar(diffCount[index]);
  out->putChar(window);
  out->putChar(index);
  for( int i = 0; i <= diffCount[index]; i++ ) {
    const int v = i == diffCount[index] ? int(len - diffPos[index * LIMIT + i]) :
                  int(diffPos[index * LIMIT + i + 1] - diffPos[index * LIMIT + i]) - 1;
    out->put32(v);
  }
  for( int i = 0; i < diffCount[index]; i++ )
    out->putChar(diffByte[index * LIMIT + i + 1]);

  in->setpos(pos_backup);
  main_strm.zalloc = Z_NULL;
  main_strm.zfree = Z_NULL;
  main_strm.opaque = Z_NULL;
  main_strm.next_in = Z_NULL;
  main_strm.avail_in = 0;
  if( zlib_inflateInit(&main_strm, zh) != Z_OK )
    return false;
  for( uint64_t i = 0; i < len; i += BLOCK ) {
    uint32_t blsize = min(uint32_t(len - i), BLOCK);
    in->blockRead(&zin[0], blsize);
    main_strm.next_in = &zin[0];
    main_strm.avail_in = blsize;
    do {
      main_strm.next_out = &zout[0];
      main_strm.avail_out = BLOCK;
      main_ret = inflate(&main_strm, Z_FINISH);
      out->blockWrite(&zout[0], BLOCK - main_strm.avail_out);
    } while( main_strm.avail_out == 0 && main_ret == Z_BUF_ERROR);
    if( main_ret != Z_BUF_ERROR && main_ret != Z_STREAM_END )
      break;
  }
  inflateEnd(&main_strm);
  hdrsize = diffCount[index] * 5 + 7;
  return main_ret == Z_STREAM_END;
}

int decode_zlib(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  const int BLOCK = 1 << 16, LIMIT = 128;
  uint8_t zin[BLOCK], zout[BLOCK];
  int diffCount = min(in->getchar(), LIMIT - 1);
  int window = in->getchar() - MAX_WBITS;
  int index = in->getchar();
  int memlevel = (index % 9) + 1;
  int clevel = (index / 9) + 1;
  int len = 0;
  int diffPos[LIMIT];
  diffPos[0] = -1;
  for( int i = 0; i <= diffCount; i++ ) {
    int v = in->get32();
    if( i == diffCount )
      len = v + diffPos[i];
    else
      diffPos[i + 1] = v + diffPos[i] + 1;
  }
  uint8_t diffByte[LIMIT];
  diffByte[0] = 0;
  for( int i = 0; i < diffCount; i++ )
    diffByte[i + 1] = in->getchar();
  size -= 7 + 5 * diffCount;

  z_stream rec_strm;
  int diffIndex = 1, recpos = 0;
  rec_strm.zalloc = Z_NULL;
  rec_strm.zfree = Z_NULL;
  rec_strm.opaque = Z_NULL;
  rec_strm.next_in = Z_NULL;
  rec_strm.avail_in = 0;
  int ret = deflateInit2(&rec_strm, clevel, Z_DEFLATED, window, memlevel, Z_DEFAULT_STRATEGY);
  if( ret != Z_OK )
    return 0;
  for( uint64_t i = 0; i < size; i += BLOCK ) {
    uint32_t blsize = min(uint32_t(size - i), BLOCK);
    in->blockRead(&zin[0], blsize);
    rec_strm.next_in = &zin[0];
    rec_strm.avail_in = blsize;
    do {
      rec_strm.next_out = &zout[0];
      rec_strm.avail_out = BLOCK;
      ret = deflate(&rec_strm, i + blsize == size ? Z_FINISH : Z_NO_FLUSH);
      if( ret != Z_BUF_ERROR && ret != Z_STREAM_END && ret != Z_OK )
        break;
      const int have = min(BLOCK - rec_strm.avail_out, len - recpos);
      while( diffIndex <= diffCount && diffPos[diffIndex] >= recpos && diffPos[diffIndex] < recpos + have ) {
        zout[diffPos[diffIndex] - recpos] = diffByte[diffIndex];
        diffIndex++;
      }
      if( mode == FDECOMPRESS )
        out->blockWrite(&zout[0], have);
      else if( mode == FCOMPARE )
        for( int j = 0; j < have; j++ )
          if( zout[j] != out->getchar() && !diffFound )
            diffFound = recpos + j + 1;
      recpos += have;

    } while( rec_strm.avail_out == 0 );
  }
  while( diffIndex <= diffCount ) {
    if( mode == FDECOMPRESS )
      out->putChar(diffByte[diffIndex]);
    else if( mode == FCOMPARE )
      if( diffByte[diffIndex] != out->getchar() && !diffFound )
        diffFound = recpos + 1;
    diffIndex++;
    recpos++;
  }
  deflateEnd(&rec_strm);
  return recpos == len ? len : 0;
}

#endif //USE_ZLIB

//
// decode/encode base64
//
static const char table1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool isBase64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/') || (c == 10) || (c == 13));
}

uint64_t decodeBase64(File *in, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t inn[3];
  int i, len = 0, blocksout = 0;
  int fle = 0;
  int linesize = 0;
  int outlen = 0;
  int tlf = 0;
  linesize = in->getchar();
  outlen = in->getchar();
  outlen += (in->getchar() << 8);
  outlen += (in->getchar() << 16);
  tlf = (in->getchar());
  outlen += ((tlf & 63) << 24);
  Array<uint8_t> ptr((outlen >> 2) * 4 + 10);
  tlf = (tlf & 192);
  if( tlf == 128 )
    tlf = 10; // LF: 10
  else if( tlf == 64 )
    tlf = 13; // LF: 13
  else
    tlf = 0;

  while( fle < outlen ) {
    len = 0;
    for( i = 0; i < 3; i++ ) {
      int c = in->getchar();
      if( c != EOF) {
        inn[i] = c;
        len++;
      } else
        inn[i] = 0;
    }
    if( len ) {
      uint8_t in0, in1, in2;
      in0 = inn[0], in1 = inn[1], in2 = inn[2];
      ptr[fle++] = (table1[in0 >> 2]);
      ptr[fle++] = (table1[((in0 & 0x03) << 4) | ((in1 & 0xf0) >> 4)]);
      ptr[fle++] = ((len > 1 ? table1[((in1 & 0x0f) << 2) | ((in2 & 0xc0) >> 6)] : '='));
      ptr[fle++] = ((len > 2 ? table1[in2 & 0x3f] : '='));
      blocksout++;
    }
    if( blocksout >= (linesize / 4) && linesize != 0 ) { //no lf if linesize==0
      if( blocksout && !in->eof() && fle <= outlen ) { //no lf if eof
        if( tlf )
          ptr[fle++] = (tlf);
        else
          ptr[fle++] = 13, ptr[fle++] = 10;
      }
      blocksout = 0;
    }
  }
  //Write out or compare
  if( mode == FDECOMPRESS ) {
    out->blockWrite(&ptr[0], outlen);
  } else if( mode == FCOMPARE ) {
    for( i = 0; i < outlen; i++ ) {
      uint8_t b = ptr[i];
      if( b != out->getchar() && !diffFound )
        diffFound = (int) out->curPos();
    }
  }
  return outlen;
}

inline char valueb(char c) {
  const char *p = strchr(table1, c);
  if( p )
    return (char) (p - table1);
  return 0;
}

void encode_base64(File *in, File *out, uint64_t len64) {
  int len = (int) len64;
  int inLen = 0;
  int i = 0;
  int j = 0;
  int b = 0;
  int lfp = 0;
  int tlf = 0;
  char src[4];
  int b64Mem = (len >> 2) * 3 + 10;
  Array<uint8_t> ptr(b64Mem);
  int olen = 5;

  while( b = in->getchar(), inLen++, (b != '=') && isBase64(b) && inLen <= len ) {
    if( b == 13 || b == 10 ) {
      if( lfp == 0 )
        lfp = inLen, tlf = b;
      if( tlf != b )
        tlf = 0;
      continue;
    }
    src[i++] = b;
    if( i == 4 ) {
      for( j = 0; j < 4; j++ )
        src[j] = valueb(src[j]);
      src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
      src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
      src[2] = ((src[2] & 0x3) << 6) + src[3];

      ptr[olen++] = src[0];
      ptr[olen++] = src[1];
      ptr[olen++] = src[2];
      i = 0;
    }
  }

  if( i ) {
    for( j = i; j < 4; j++ )
      src[j] = 0;

    for( j = 0; j < 4; j++ )
      src[j] = valueb(src[j]);

    src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
    src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
    src[2] = ((src[2] & 0x3) << 6) + src[3];

    for( j = 0; (j < i - 1); j++ ) {
      ptr[olen++] = src[j];
    }
  }
  ptr[0] = lfp & 255; //nl length
  ptr[1] = len & 255;
  ptr[2] = (len >> 8) & 255;
  ptr[3] = (len >> 16) & 255;
  if( tlf != 0 ) {
    if( tlf == 10 )
      ptr[4] = 128;
    else
      ptr[4] = 64;
  } else
    ptr[4] = (len >> 24) & 63; //1100 0000
  out->blockWrite(&ptr[0], olen);
}

#define LZW_TABLE_SIZE 9221

#define lzw_find(k) \
  { \
    offset     = ( int ) finalize64(k * PHI64, 13); \
    int stride = (offset > 0) ? LZW_TABLE_SIZE - offset : 1; \
    while (true) { \
      if ((index = table[offset]) < 0) { \
        index = -offset - 1; \
        break; \
      } else if (dict[index] == int(k)) { \
        break; \
      } \
      offset -= stride; \
      if (offset < 0) \
        offset += LZW_TABLE_SIZE; \
    } \
  }

#define lzw_reset \
  { \
    for (int i = 0; i < LZW_TABLE_SIZE; table[i] = -1, i++) \
      ; \
  }

int encode_gif(File *in, File *out, uint64_t len, int &hdrsize) {
  int codesize = in->getchar(), diffpos = 0, clearpos = 0, bsize = 0, code, offset = 0;
  uint64_t beginin = in->curPos(), beginout = out->curPos();
  Array<uint8_t> output(4096);
  hdrsize = 6;
  out->putChar(hdrsize >> 8);
  out->putChar(hdrsize & 255);
  out->putChar(bsize);
  out->putChar(clearpos >> 8);
  out->putChar(clearpos & 255);
  out->putChar(codesize);
  Array<int> table(LZW_TABLE_SIZE);
  for( int phase = 0; phase < 2; phase++ ) {
    in->setpos(beginin);
    int bits = codesize + 1, shift = 0, buffer = 0;
    int blockSize = 0, maxcode = (1 << codesize) + 1, last = -1;
    Array<int> dict(4096);
    lzw_reset
    bool end = false;
    while((blockSize = in->getchar()) > 0 && in->curPos() - beginin < len && !end ) {
      for( int i = 0; i < blockSize; i++ ) {
        buffer |= in->getchar() << shift;
        shift += 8;
        while( shift >= bits && !end ) {
          code = buffer & ((1 << bits) - 1);
          buffer >>= bits;
          shift -= bits;
          if( !bsize && code != (1 << codesize)) {
            hdrsize += 4;
            out->put32(0);
          }
          if( !bsize )
            bsize = blockSize;
          if( code == (1 << codesize)) {
            if( maxcode > (1 << codesize) + 1 ) {
              if( clearpos && clearpos != 69631 - maxcode )
                return 0;
              clearpos = 69631 - maxcode;
            }
            bits = codesize + 1, maxcode = (1 << codesize) + 1, last = -1;
            lzw_reset
          } else if( code == (1 << codesize) + 1 )
            end = true;
          else if( code > maxcode + 1 )
            return 0;
          else {
            int j = (code <= maxcode ? code : last), size = 1;
            while( j >= (1 << codesize)) {
              output[4096 - (size++)] = dict[j] & 255;
              j = dict[j] >> 8;
            }
            output[4096 - size] = j;
            if( phase == 1 )
              out->blockWrite(&output[4096 - size], size);
            else
              diffpos += size;
            if( code == maxcode + 1 ) {
              if( phase == 1 )
                out->putChar(j);
              else
                diffpos++;
            }
            if( last != -1 ) {
              if( ++maxcode >= 8191 )
                return 0;
              if( maxcode <= 4095 ) {
                int key = (last << 8) + j, index = -1;
                lzw_find(key)
                dict[maxcode] = key;
                table[(index < 0) ? -index - 1 : offset] = maxcode;
                if( phase == 0 && index > 0 ) {
                  hdrsize += 4;
                  j = diffpos - size - (code == maxcode);
                  out->put32(j);
                  diffpos = size + (code == maxcode);
                }
              }
              if( maxcode >= ((1 << bits) - 1) && bits < 12 )
                bits++;
            }
            last = code;
          }
        }
      }
    }
  }
  diffpos = (int) out->curPos();
  out->setpos(beginout);
  out->putChar(hdrsize >> 8);
  out->putChar(hdrsize & 255);
  out->putChar(255 - bsize);
  out->putChar((clearpos >> 8) & 255);
  out->putChar(clearpos & 255);
  out->setpos(diffpos);
  return in->curPos() - beginin == len - 1;
}

#define gif_write_block(count) \
  { \
    output[0] = (count); \
    if (mode == FDECOMPRESS) \
      out->blockWrite(&output[0], (count) + 1); \
    else if (mode == FCOMPARE) \
      for (int j = 0; j < (count) + 1; j++) \
        if (output[j] != out->getchar() && ! diffFound) { \
          diffFound = outsize + j + 1; \
          return 1; \
        } \
    outsize += (count) + 1; \
    blockSize = 0; \
  }

#define gif_write_code(c) \
  { \
    buffer += (c) << shift; \
    shift += bits; \
    while (shift >= 8) { \
      output[++blockSize] = buffer & 255; \
      buffer >>= 8; \
      shift -= 8; \
      if (blockSize == bsize) gif_write_block(bsize); \
    } \
  }

int decode_gif(File *in, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  int diffcount = in->getchar(), curdiff = 0;
  Array<int> diffpos(4096);
  diffcount = ((diffcount << 8) + in->getchar() - 6) / 4;
  int bsize = 255 - in->getchar();
  int clearpos = in->getchar();
  clearpos = (clearpos << 8) + in->getchar();
  clearpos = (69631 - clearpos) & 0xffff;
  int codesize = in->getchar(), bits = codesize + 1, shift = 0, buffer = 0, blockSize = 0;
  if( diffcount > 4096 || clearpos <= (1 << codesize) + 2 )
    return 1;
  int maxcode = (1 << codesize) + 1, input, code, offset = 0;
  Array<int> dict(4096);
  Array<int> table(LZW_TABLE_SIZE);
  lzw_reset
  for( int i = 0; i < diffcount; i++ ) {
    diffpos[i] = in->getchar();
    diffpos[i] = (diffpos[i] << 8) + in->getchar();
    diffpos[i] = (diffpos[i] << 8) + in->getchar();
    diffpos[i] = (diffpos[i] << 8) + in->getchar();
    if( i > 0 )
      diffpos[i] += diffpos[i - 1];
  }
  Array<uint8_t> output(256);
  size -= 6 + diffcount * 4;
  int last = in->getchar(), total = (int) size + 1, outsize = 1;
  if( mode == FDECOMPRESS )
    out->putChar(codesize);
  else if( mode == FCOMPARE )
    if( codesize != out->getchar() && !diffFound )
      diffFound = 1;
  if( diffcount == 0 || diffpos[0] != 0 ) gif_write_code(1 << codesize) else
    curdiff++;
  while( size != 0 && (input = in->getchar()) != EOF) {
    size--;
    int key = (last << 8) + input, index = (code = -1);
    if( last < 0 )
      index = input;
    else lzw_find(key)
    code = index;
    if( curdiff < diffcount && total - (int) size > diffpos[curdiff] )
      curdiff++, code = -1;
    if( code < 0 ) {
      gif_write_code(last)
      if( maxcode == clearpos ) {
        gif_write_code(1 << codesize)
        bits = codesize + 1, maxcode = (1 << codesize) + 1;
        lzw_reset
      } else {
        ++maxcode;
        if( maxcode <= 4095 ) {
          dict[maxcode] = key;
          table[(index < 0) ? -index - 1 : offset] = maxcode;
        }
        if( maxcode >= (1 << bits) && bits < 12 )
          bits++;
      }
      code = input;
    }
    last = code;
  }
  gif_write_code(last)
  gif_write_code((1 << codesize) + 1)
  if( shift > 0 ) {
    output[++blockSize] = buffer & 255;
    if( blockSize == bsize ) gif_write_block(bsize)
  }
  if( blockSize > 0 ) gif_write_block(blockSize)
  if( mode == FDECOMPRESS )
    out->putChar(0);
  else if( mode == FCOMPARE )
    if( 0 != out->getchar() && !diffFound )
      diffFound = outsize + 1;
  return outsize + 1;
}


//////////////////// Compress, Decompress ////////////////////////////

void direct_encode_block(BlockType type, File *in, uint64_t len, Encoder &en, int info = -1) {
  //TODO: Large file support
  en.compress(type);
  en.encodeBlockSize(len);
  if( info != -1 ) {
    en.compress((info >> 24) & 0xFF);
    en.compress((info >> 16) & 0xFF);
    en.compress((info >> 8) & 0xFF);
    en.compress((info) & 0xFF);
  }
  fprintf(stderr, "Compressing... ");
  for( uint64_t j = 0; j < len; ++j ) {
    if((j & 0xfff) == 0 )
      en.printStatus(j, len);
    en.compress(in->getchar());
  }
  fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

void compressRecursive(File *in, uint64_t blockSize, Encoder &en, String &blstr, int recursionLevel, float p1, float p2);

uint64_t decode_func(BlockType type, Encoder &en, File *tmp, uint64_t len, int info, File *out, FMode mode, uint64_t &diffFound) {
  if( type == IMAGE24 )
    return decode_bmp(en, len, info, out, mode, diffFound);
  else if( type == IMAGE32 )
    return decode_im32(en, len, info, out, mode, diffFound);
  else if( type == AUDIO_LE )
    return decode_endianness16b(en, len, out, mode, diffFound);
  else if( type == EXE )
    return decode_exe(en, len, out, mode, diffFound);
  else if( type == TEXT_EOL )
    return decode_eol(en, len, out, mode, diffFound);
  else if( type == CD )
    return decode_cd(tmp, len, out, mode, diffFound);
#ifdef USE_ZLIB
  else if( type == ZLIB )
    return decode_zlib(tmp, len, out, mode, diffFound);
#endif //USE_ZLIB
  else if( type == BASE64 )
    return decodeBase64(tmp, out, mode, diffFound);
  else if( type == GIF )
    return decode_gif(tmp, len, out, mode, diffFound);
  else if( type == RLE )
    return decode_rle(tmp, len, out, mode, diffFound);
  else if( type == LZW )
    return decode_lzw(tmp, len, out, mode, diffFound);
  else
    assert(false);
  return 0;
}

uint64_t encode_func(BlockType type, File *in, File *tmp, uint64_t len, int info, int &hdrsize) {
  if( type == IMAGE24 )
    encode_bmp(in, tmp, len, info);
  else if( type == IMAGE32 )
    encode_im32(in, tmp, len, info);
  else if( type == AUDIO_LE )
    encode_endianness16b(in, tmp, len);
  else if( type == EXE )
    encode_exe(in, tmp, len, info);
  else if( type == TEXT_EOL )
    encode_eol(in, tmp, len);
  else if( type == CD )
    encode_cd(in, tmp, len, info);
#ifdef USE_ZLIB
  else if( type == ZLIB )
    return encode_zlib(in, tmp, len, hdrsize) ? 0 : 1;
#endif //USE_ZLIB
  else if( type == BASE64 )
    encode_base64(in, tmp, len);
  else if( type == GIF )
    return encode_gif(in, tmp, len, hdrsize) ? 0 : 1;
  else if( type == RLE )
    encode_rle(in, tmp, len, info, hdrsize);
  else if( type == LZW )
    return encode_lzw(in, tmp, len, hdrsize) ? 0 : 1;
  else
    assert(false);
  return 0;
}

void transform_encode_block(BlockType type, File *in, uint64_t len, Encoder &en, int info, String &blstr, int recursion_level, float p1,
                            float p2, uint64_t begin) {
  if( hasTransform(type)) {
    FileTmp tmp;
    int hdrsize = 0;
    uint64_t diffFound = encode_func(type, in, &tmp, len, info, hdrsize);
    const uint64_t tmpsize = tmp.curPos();
    tmp.setpos(tmpsize); //switch to read mode
    if( diffFound == 0 ) {
      tmp.setpos(0);
      en.setFile(&tmp);
      in->setpos(begin);
      decode_func(type, en, &tmp, tmpsize, info, in, FCOMPARE, diffFound);
    }
    // Test fails, compress without transform
    if( diffFound > 0 || tmp.getchar() != EOF) {
      printf("Transform fails at %" PRIu64 ", skipping...\n", diffFound - 1);
      in->setpos(begin);
      direct_encode_block(DEFAULT, in, len, en);
    } else {
      tmp.setpos(0);
      if( hasRecursion(type)) {
        //TODO: Large file support
        en.compress(type);
        en.encodeBlockSize(tmpsize);
        BlockType type2 = (BlockType) ((info >> 24) & 0xFF);
        if( type2 != DEFAULT ) {
          String blstr_sub0;
          blstr_sub0 += blstr.c_str();
          blstr_sub0 += "->";
          String blstr_sub1;
          blstr_sub1 += blstr.c_str();
          blstr_sub1 += "-->";
          String blstr_sub2;
          blstr_sub2 += blstr.c_str();
          blstr_sub2 += "-->";
          printf(" %-11s | ->  exploded     |%10d bytes [%d - %d]\n", blstr_sub0.c_str(), int(tmpsize), 0, int(tmpsize - 1));
          printf(" %-11s | --> added header |%10d bytes [%d - %d]\n", blstr_sub1.c_str(), hdrsize, 0, hdrsize - 1);
          direct_encode_block(HDR, &tmp, hdrsize, en);
          printf(" %-11s | --> data         |%10d bytes [%d - %d]\n", blstr_sub2.c_str(), int(tmpsize - hdrsize), hdrsize,
                 int(tmpsize - 1));
          transform_encode_block(type2, &tmp, tmpsize - hdrsize, en, info & 0xffffff, blstr, recursion_level, p1, p2, hdrsize);
        } else {
          compressRecursive(&tmp, tmpsize, en, blstr, recursion_level + 1, p1, p2);
        }
      } else {
        direct_encode_block(type, &tmp, tmpsize, en, hasInfo(type) ? info : -1);
      }
    }
    tmp.close();
  } else {
    direct_encode_block(type, in, len, en, hasInfo(type) ? info : -1);
  }
}

void compressRecursive(File *in, const uint64_t blockSize, Encoder &en, String &blstr, int recursionLevel, float p1, float p2) {
  static const char *typenames[25] = {"default", "filecontainer", "jpeg", "hdr", "1b-image", "4b-image", "8b-image", "8b-img-grayscale",
                                      "24b-image", "32b-image", "audio", "audio - le", "exe", "cd", "zlib", "base64", "gif", "png-8b",
                                      "png-8b-grayscale", "png-24b", "png-32b", "text", "text - eol", "rle", "lzw"};
  static const char *audiotypes[4] = {"8b-mono", "8b-stereo", "16b-mono", "16b-stereo"};
  BlockType type = DEFAULT;
  int blnum = 0, info = 0; // image width or audio type
  uint64_t begin = in->curPos();
  uint64_t block_end = begin + blockSize;
  if( recursionLevel == 5 ) {
    direct_encode_block(DEFAULT, in, blockSize, en);
    return;
  }
  float pscale = blockSize > 0 ? (p2 - p1) / blockSize : 0;

  // Transform and test in blocks
  uint64_t nextblockStart;
  uint64_t textStart;
  uint64_t textEnd = 0;
  BlockType nextblockType;
  BlockType nextblockTypeBak = DEFAULT; //initialized only to suppress a compiler warning, will be overwritten
  uint64_t bytesToGo = blockSize;
  while( bytesToGo > 0 ) {
    if( type == TEXT || type == TEXT_EOL ) { // it was a split block in the previous iteration: TEXT -> DEFAULT -> ...
      nextblockType = nextblockTypeBak;
      nextblockStart = textEnd + 1;
    } else {
      nextblockType = detect(in, bytesToGo, type, info);
      nextblockStart = in->curPos();
      in->setpos(begin);
    }

    // override (any) next block detection by a preceding text block
    textStart = begin + textparser._start[0];
    textEnd = begin + textparser._end[0];
    if( textEnd > nextblockStart - 1 )
      textEnd = nextblockStart - 1;
    if( type == DEFAULT && textStart < textEnd ) { // only DEFAULT blocks may be overridden
      if( textStart == begin && textEnd == nextblockStart - 1 ) { // whole first block is text
        type = (textparser._EOLType[0] == 1) ? TEXT_EOL : TEXT; // DEFAULT -> TEXT
      } else if( textEnd - textStart + 1 >= TEXT_MIN_SIZE ) { // we have one (or more) large enough text portion that splits DEFAULT
        if( textStart != begin ) { // text is not the first block
          nextblockStart = textStart; // first block is still DEFAULT
          nextblockTypeBak = nextblockType;
          nextblockType = (textparser._EOLType[0] == 1) ? TEXT_EOL : TEXT; //next block is text
          textparser.removefirst();
        } else {
          type = (textparser._EOLType[0] == 1) ? TEXT_EOL : TEXT; // first block is text
          nextblockType = DEFAULT; // next block is DEFAULT
          nextblockStart = textEnd + 1;
        }
      }
      // no text block is found, still DEFAULT
    }

    if( nextblockStart > block_end ) { // if a detection reports a larger size than the actual block size, fall back
      nextblockStart = begin + 1;
      type = nextblockType = DEFAULT;
    }

    uint64_t len = nextblockStart - begin;
    if( len > 0 ) {
      en.setStatusRange(p1, p2 = p1 + pscale * len);

      //Compose block enumeration string
      String blstrSub;
      blstrSub += blstr.c_str();
      if( blstrSub.strsize() != 0 )
        blstrSub += "-";
      blstrSub += uint64_t(blnum);
      blnum++;

      printf(" %-11s | %-16s |%10" PRIu64 " bytes [%" PRIu64 " - %" PRIu64 "]", blstrSub.c_str(),
             typenames[(type == ZLIB && isPNG(BlockType(info >> 24))) ? info >> 24 : type], len, begin, nextblockStart - 1);
      if( type == AUDIO || type == AUDIO_LE )
        printf(" (%s)", audiotypes[info % 4]);
      else if( type == IMAGE1 || type == IMAGE4 || type == IMAGE8 || type == IMAGE8GRAY || type == IMAGE24 || type == IMAGE32 ||
               (type == ZLIB && isPNG(BlockType(info >> 24))))
        printf(" (width: %d)", (type == ZLIB) ? (info & 0xFFFFFF) : info);
      else if( hasRecursion(type) && (info >> 24) != DEFAULT )
        printf(" (%s)", typenames[info >> 24]);
      else if( type == CD )
        printf(" (mode%d/form%d)", info == 1 ? 1 : 2, info != 3 ? 1 : 2);
      printf("\n");
      transform_encode_block(type, in, len, en, info, blstrSub, recursionLevel, p1, p2, begin);
      p1 = p2;
      bytesToGo -= len;
    }
    type = nextblockType;
    begin = nextblockStart;
  }
}

// Compress a file. Split fileSize bytes into blocks by type.
// For each block, output
// <type> <size> and call encode_X to convert to type X.
// Test transform and compress.
void compressfile(const char *filename, uint64_t fileSize, Encoder &en, bool verbose) {
  assert(en.getMode() == COMPRESS);
  assert(filename && filename[0]);

  en.compress(FILECONTAINER);
  uint64_t start = en.size();
  en.encodeBlockSize(fileSize);

  FileDisk in;
  in.open(filename, true);
  printf("Block segmentation:\n");
  String blstr;
  compressRecursive(&in, fileSize, en, blstr, 0, 0.0f, 1.0f);
  in.close();

  if( options & OPTION_MULTIPLE_FILE_MODE ) { //multiple file mode
    if( verbose )
      printf("File size to encode   : 4\n"); //This string must be long enough. "Compressing ..." is still on screen, we need to overwrite it.
    printf("File input size       : %" PRIu64 "\n", fileSize);
    printf("File compressed size  : %" PRIu64 "\n", en.size() - start);
  }
}

uint64_t decompressRecursive(File *out, uint64_t blockSize, Encoder &en, FMode mode, int recursionLevel) {
  BlockType type;
  uint64_t len, i = 0;
  uint64_t diffFound = 0;
  int info = 0;
  while( i < blockSize ) {
    type = (BlockType) en.decompress();
    len = en.decodeBlockSize();
    if( hasInfo(type)) {
      info = 0;
      for( int j = 0; j < 4; ++j ) {
        info <<= 8;
        info += en.decompress();
      }
    }
    if( hasRecursion(type)) {
      FileTmp tmp;
      decompressRecursive(&tmp, len, en, FDECOMPRESS, recursionLevel + 1);
      if( mode != FDISCARD ) {
        tmp.setpos(0);
        if( hasTransform(type))
          len = decode_func(type, en, &tmp, len, info, out, mode, diffFound);
      }
      tmp.close();
    } else if( hasTransform(type)) {
      len = decode_func(type, en, NULL, len, info, out, mode, diffFound);
    } else {
      for( uint64_t j = 0; j < len; ++j ) {
        if( !(j & 0xfff))
          en.printStatus();
        if( mode == FDECOMPRESS )
          out->putChar(en.decompress());
        else if( mode == FCOMPARE ) {
          if( en.decompress() != out->getchar() && !diffFound ) {
            mode = FDISCARD;
            diffFound = i + j + 1;
          }
        } else
          en.decompress();
      }
    }
    i += len;
  }
  return diffFound;
}

// Decompress or compare a file
void decompressFile(const char *filename, FMode fmode, Encoder &en) {
  assert(en.getMode() == DECOMPRESS);
  assert(filename && filename[0]);

  BlockType blocktype = (BlockType) en.decompress();
  if( blocktype != FILECONTAINER )
    quit("Bad archive.");
  uint64_t fileSize = en.decodeBlockSize();

  FileDisk f;
  if( fmode == FCOMPARE ) {
    f.open(filename, true);
    printf("Comparing");
  } else { //mode==FDECOMPRESS;
    f.create(filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, fileSize);

  // Decompress/Compare
  uint64_t r = decompressRecursive(&f, fileSize, en, fmode, 0);
  if( fmode == FCOMPARE && !r && f.getchar() != EOF)
    printf("file is longer\n");
  else if( fmode == FCOMPARE && r )
    printf("differ at %" PRIu64 "\n", r - 1);
  else if( fmode == FCOMPARE )
    printf("identical\n");
  else
    printf("done   \n");
  f.close();
}

#endif //PAQ8PX_FILTERS_HPP
