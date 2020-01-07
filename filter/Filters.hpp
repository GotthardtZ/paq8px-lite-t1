#ifndef PAQ8PX_FILTERS_HPP
#define PAQ8PX_FILTERS_HPP

#include <cstdint>
#include <cctype>
#include <cstring>
#include "../UTF8.hpp"
#include "../Encoder.hpp"
#include "../File.hpp"
#include "../utils.hpp"
#include "../Array.hpp"

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


typedef enum {
    FDECOMPRESS, FCOMPARE, FDISCARD
} FMode;

#include "ecc.hpp"
#include "cd.hpp"

#ifdef USE_ZLIB

#include "zlib.hpp"

#endif //USE_ZLIB


#include "lzw.hpp"
#include "base64.hpp"
#include "gif.hpp"

#define IMG_DET(type, start_pos, header_len, width, height) return dett = (type), \
                                                                   deth = (header_len), detd = (width) * (height), info = (width), \
                                                                   in->setpos(start + (start_pos)), HDR

#define AUD_DET(type, start_pos, header_len, data_len, wmode) return dett = (type), \
                                                                     deth = (header_len), detd = (data_len), info = (wmode), \
                                                                     in->setpos(start + (start_pos)), HDR


bool isGrayscalePalette(File *in, int n = 256, int isRGBA = 0) {
  uint64_t offset = in->curPos();
  int stride = 3 + isRGBA, res = (n > 0) << 8U, order = 1;
  for( int i = 0; (i < n * stride) && (res >> 8U); i++ ) {
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

#include "TextParserStateInfo.hpp"

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
  unsigned char zbuf[256 + 32] = {0}, zin[1U << 16] = {0}, zout[1 << 16] = {0}; // For ZLIB stream detection
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

  textParser.reset(0);
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
        nextchunk += buf1 + 4 /*CRC*/ + 8 /*Chunk length+id*/;
        lastchunk = buf0;
        png *= (lastchunk != 0x49454E44 /*IEND*/);
        if( lastchunk == 0x504C5445 /*PLTE*/) {
          png *= (buf1 % 3 == 0);
          pnggray = (png && isGrayscalePalette(in, buf1 / 3));
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

    int zh = parseZlibHeader(((int) zbuf[(zbufpos - 32) & 0xFF]) * 256 + (int) zbuf[(zbufpos - 32 + 1) & 0xFF]);
    bool valid = (i >= 31 && zh != -1);
    if( !valid && options & OPTION_BRUTE && i >= 255 ) {
      uint8_t bType = (zbuf[zbufpos] & 7) >> 1;
      if((valid = (bType == 1 || bType == 2))) {
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
      if( zlibInflateInit(&strm, zh) == Z_OK ) {
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
        if( zlibInflateInit(&strm, zh) == Z_OK ) {
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
        int t = expandCdSector(data, cda, 1);
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
            if( isGrayscalePalette(in, n_colors, 1))
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
        if( textParser.start() == uint32_t(i) || // for any sign of non-text data in pixel area
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
            IMG_DET((isGrayscalePalette(in)) ? IMAGE8GRAY : IMAGE8, tga - 7, 18 + tgaid + 256 * tgamap, tgax, tgay);
          } else if( tgat == 2 )
            IMG_DET((tgaz == 24) ? IMAGE24 : IMAGE32, tga - 7, 18 + tgaid, tgax * (tgaz >> 3), tgay);
          else if( tgat == 3 )
            IMG_DET(IMAGE8GRAY, tga - 7, 18 + tgaid, tgax, tgay);
          else if( tgat == 9 || tgat == 11 ) {
            const uint64_t savedpos = in->curPos();
            in->setpos(start + tga + 11 + tgaid);
            if( tgat == 9 ) {
              info = (isGrayscalePalette(in) ? IMAGE8GRAY : IMAGE8) << 24;
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
        gifgray = isGrayscalePalette(in, gifplt / 3), gifplt = 0;
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
        gifgray = isGrayscalePalette(in, gifplt / 3), gifplt = 0;
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
    textParser.UTF8State = utf8StateTable[256 + textParser.UTF8State + t];
    if( textParser.UTF8State == UTF8_ACCEPT ) { // proper end of a valid utf8 sequence
      if( c == NEW_LINE ) {
        if(((buf0 >> 8) & 0xff) != CARRIAGE_RETURN )
          textParser.setEolType(2); // mixed or LF-only
        else if( textParser.eolType() == 0 )
          textParser.setEolType(1); // CRLF-only
      }
      textParser.invalidCount = textParser.invalidCount * (TEXT_ADAPT_RATE - 1) / TEXT_ADAPT_RATE;
      if( textParser.invalidCount == 0 )
        textParser.setEnd(i); // a possible end of block position
    } else if( textParser.UTF8State == UTF8_REJECT ) { // illegal state
      textParser.invalidCount = textParser.invalidCount * (TEXT_ADAPT_RATE - 1) / TEXT_ADAPT_RATE + TEXT_ADAPT_RATE;
      textParser.UTF8State = UTF8_ACCEPT; // reset state
      if( textParser.validLength() < TEXT_MIN_SIZE ) {
        textParser.reset(i + 1); // it's not text (or not long enough) - start over
      } else if( textParser.invalidCount >= TEXT_MAX_MISSES * TEXT_ADAPT_RATE ) {
        if( textParser.validLength() < TEXT_MIN_SIZE )
          textParser.reset(i + 1); // it's not text (or not long enough) - start over
        else // Commit text block validation
          textParser.next(i + 1);
      }
    }
  }
  return type;
}

#include "bmp.hpp"
#include "im32.hpp"
#include "endianness16b.hpp"
#include "eol.hpp"
#include "rle.hpp"
#include "exe.hpp"

//////////////////// Compress, Decompress ////////////////////////////

void directEncodeBlock(BlockType type, File *in, uint64_t len, Encoder &en, int info = -1) {
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

uint64_t decodeFunc(BlockType type, Encoder &en, File *tmp, uint64_t len, int info, File *out, FMode mode, uint64_t &diffFound) {
  if( type == IMAGE24 )
    return decodeBmp(en, len, info, out, mode, diffFound);
  else if( type == IMAGE32 )
    return decodeIm32(en, len, info, out, mode, diffFound);
  else if( type == AUDIO_LE )
    return decodeEndianness16B(en, len, out, mode, diffFound);
  else if( type == EXE )
    return decodeExe(en, len, out, mode, diffFound);
  else if( type == TEXT_EOL )
    return decodeEol(en, len, out, mode, diffFound);
  else if( type == CD )
    return decodeCd(tmp, len, out, mode, diffFound);
#ifdef USE_ZLIB
  else if( type == ZLIB )
    return decodeZlib(tmp, len, out, mode, diffFound);
#endif //USE_ZLIB
  else if( type == BASE64 )
    return decodeBase64(tmp, out, mode, diffFound);
  else if( type == GIF )
    return decodeGif(tmp, len, out, mode, diffFound);
  else if( type == RLE ) {
    auto r = new RleFilter();
    return r->decode(tmp, out, mode, len, diffFound);
  } else if( type == LZW )
    return decodeLzw(tmp, len, out, mode, diffFound);
  else
    assert(false);
  return 0;
}

uint64_t encodeFunc(BlockType type, File *in, File *tmp, uint64_t len, int info, int &hdrsize) {
  if( type == IMAGE24 )
    encodeBmp(in, tmp, len, info);
  else if( type == IMAGE32 )
    encodeIm32(in, tmp, len, info);
  else if( type == AUDIO_LE )
    encodeEndianness16B(in, tmp, len);
  else if( type == EXE )
    encodeExe(in, tmp, len, info);
  else if( type == TEXT_EOL )
    encodeEol(in, tmp, len);
  else if( type == CD )
    encodeCd(in, tmp, len, info);
#ifdef USE_ZLIB
  else if( type == ZLIB )
    return encodeZlib(in, tmp, len, hdrsize) ? 0 : 1;
#endif //USE_ZLIB
  else if( type == BASE64 )
    encodeBase64(in, tmp, len);
  else if( type == GIF )
    return encodeGif(in, tmp, len, hdrsize) ? 0 : 1;
  else if( type == RLE ) {
    auto r = new RleFilter();
    r->encode(in, tmp, len, info, hdrsize);
  }
  else if( type == LZW )
    return encodeLzw(in, tmp, len, hdrsize) ? 0 : 1;
  else
    assert(false);
  return 0;
}

void
transformEncodeBlock(BlockType type, File *in, uint64_t len, Encoder &en, int info, String &blstr, int recursionLevel, float p1, float p2,
                     uint64_t begin) {
  if( hasTransform(type)) {
    FileTmp tmp;
    int hdrsize = 0;
    uint64_t diffFound = encodeFunc(type, in, &tmp, len, info, hdrsize);
    const uint64_t tmpsize = tmp.curPos();
    tmp.setpos(tmpsize); //switch to read mode
    if( diffFound == 0 ) {
      tmp.setpos(0);
      en.setFile(&tmp);
      in->setpos(begin);
      decodeFunc(type, en, &tmp, tmpsize, info, in, FCOMPARE, diffFound);
    }
    // Test fails, compress without transform
    if( diffFound > 0 || tmp.getchar() != EOF) {
      printf("Transform fails at %" PRIu64 ", skipping...\n", diffFound - 1);
      in->setpos(begin);
      directEncodeBlock(DEFAULT, in, len, en);
    } else {
      tmp.setpos(0);
      if( hasRecursion(type)) {
        //TODO: Large file support
        en.compress(type);
        en.encodeBlockSize(tmpsize);
        BlockType type2 = (BlockType) ((info >> 24) & 0xFF);
        if( type2 != DEFAULT ) {
          String blstrSub0;
          blstrSub0 += blstr.c_str();
          blstrSub0 += "->";
          String blstrSub1;
          blstrSub1 += blstr.c_str();
          blstrSub1 += "-->";
          String blstrSub2;
          blstrSub2 += blstr.c_str();
          blstrSub2 += "-->";
          printf(" %-11s | ->  exploded     |%10d bytes [%d - %d]\n", blstrSub0.c_str(), int(tmpsize), 0, int(tmpsize - 1));
          printf(" %-11s | --> added header |%10d bytes [%d - %d]\n", blstrSub1.c_str(), hdrsize, 0, hdrsize - 1);
          directEncodeBlock(HDR, &tmp, hdrsize, en);
          printf(" %-11s | --> data         |%10d bytes [%d - %d]\n", blstrSub2.c_str(), int(tmpsize - hdrsize), hdrsize,
                 int(tmpsize - 1));
          transformEncodeBlock(type2, &tmp, tmpsize - hdrsize, en, info & 0xffffff, blstr, recursionLevel, p1, p2, hdrsize);
        } else {
          compressRecursive(&tmp, tmpsize, en, blstr, recursionLevel + 1, p1, p2);
        }
      } else {
        directEncodeBlock(type, &tmp, tmpsize, en, hasInfo(type) ? info : -1);
      }
    }
    tmp.close();
  } else {
    directEncodeBlock(type, in, len, en, hasInfo(type) ? info : -1);
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
  uint64_t blockEnd = begin + blockSize;
  if( recursionLevel == 5 ) {
    directEncodeBlock(DEFAULT, in, blockSize, en);
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
    textStart = begin + textParser._start[0];
    textEnd = begin + textParser._end[0];
    if( textEnd > nextblockStart - 1 )
      textEnd = nextblockStart - 1;
    if( type == DEFAULT && textStart < textEnd ) { // only DEFAULT blocks may be overridden
      if( textStart == begin && textEnd == nextblockStart - 1 ) { // whole first block is text
        type = (textParser._EOLType[0] == 1) ? TEXT_EOL : TEXT; // DEFAULT -> TEXT
      } else if( textEnd - textStart + 1 >= TEXT_MIN_SIZE ) { // we have one (or more) large enough text portion that splits DEFAULT
        if( textStart != begin ) { // text is not the first block
          nextblockStart = textStart; // first block is still DEFAULT
          nextblockTypeBak = nextblockType;
          nextblockType = (textParser._EOLType[0] == 1) ? TEXT_EOL : TEXT; //next block is text
          textParser.removeFirst();
        } else {
          type = (textParser._EOLType[0] == 1) ? TEXT_EOL : TEXT; // first block is text
          nextblockType = DEFAULT; // next block is DEFAULT
          nextblockStart = textEnd + 1;
        }
      }
      // no text block is found, still DEFAULT
    }

    if( nextblockStart > blockEnd ) { // if a detection reports a larger size than the actual block size, fall back
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
             typenames[(type == ZLIB && isPNG(BlockType(info >> 24U))) ? info >> 24U : type], len, begin, nextblockStart - 1);
      if( type == AUDIO || type == AUDIO_LE )
        printf(" (%s)", audiotypes[info % 4]);
      else if( type == IMAGE1 || type == IMAGE4 || type == IMAGE8 || type == IMAGE8GRAY || type == IMAGE24 || type == IMAGE32 ||
               (type == ZLIB && isPNG(BlockType(info >> 24U))))
        printf(" (width: %d)", (type == ZLIB) ? (info & 0xFFFFFFU) : info);
      else if( hasRecursion(type) && (info >> 24U) != DEFAULT )
        printf(" (%s)", typenames[info >> 24U]);
      else if( type == CD )
        printf(" (mode%d/form%d)", info == 1 ? 1 : 2, info != 3 ? 1 : 2);
      printf("\n");
      transformEncodeBlock(type, in, len, en, info, blstrSub, recursionLevel, p1, p2, begin);
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
          len = decodeFunc(type, en, &tmp, len, info, out, mode, diffFound);
      }
      tmp.close();
    } else if( hasTransform(type)) {
      len = decodeFunc(type, en, NULL, len, info, out, mode, diffFound);
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
void decompressFile(const char *filename, FMode fMode, Encoder &en) {
  assert(en.getMode() == DECOMPRESS);
  assert(filename && filename[0]);

  BlockType blocktype = (BlockType) en.decompress();
  if( blocktype != FILECONTAINER )
    quit("Bad archive.");
  uint64_t fileSize = en.decodeBlockSize();

  FileDisk f;
  if( fMode == FCOMPARE ) {
    f.open(filename, true);
    printf("Comparing");
  } else { //mode==FDECOMPRESS;
    f.create(filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, fileSize);

  // Decompress/Compare
  uint64_t r = decompressRecursive(&f, fileSize, en, fMode, 0);
  if( fMode == FCOMPARE && !r && f.getchar() != EOF)
    printf("file is longer\n");
  else if( fMode == FCOMPARE && r )
    printf("differ at %" PRIu64 "\n", r - 1);
  else if( fMode == FCOMPARE )
    printf("identical\n");
  else
    printf("done   \n");
  f.close();
}

#endif //PAQ8PX_FILTERS_HPP
