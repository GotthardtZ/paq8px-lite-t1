#ifndef PAQ8PX_BASE64_HPP
#define PAQ8PX_BASE64_HPP

static constexpr char table1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool isBase64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/') || (c == 10) || (c == 13));
}

uint64_t decodeBase64(File *in, File *out, FMode mode, uint64_t &diffFound) {
  uint8_t inn[3];
  int i, len = 0, blocksOut = 0;
  int fle = 0;
  int lineSize = 0;
  int outLen = 0;
  int tlf = 0;
  lineSize = in->getchar();
  outLen = in->getchar();
  outLen += (in->getchar() << 8);
  outLen += (in->getchar() << 16);
  tlf = (in->getchar());
  outLen += ((tlf & 63) << 24);
  Array<uint8_t> ptr((outLen >> 2) * 4 + 10);
  tlf = (tlf & 192);
  if( tlf == 128 )
    tlf = 10; // LF: 10
  else if( tlf == 64 )
    tlf = 13; // LF: 13
  else
    tlf = 0;

  while( fle < outLen ) {
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
      blocksOut++;
    }
    if( blocksOut >= (lineSize / 4) && lineSize != 0 ) { //no lf if lineSize==0
      if( blocksOut && !in->eof() && fle <= outLen ) { //no lf if eof
        if( tlf )
          ptr[fle++] = (tlf);
        else
          ptr[fle++] = 13, ptr[fle++] = 10;
      }
      blocksOut = 0;
    }
  }
  //Write out or compare
  if( mode == FDECOMPRESS ) {
    out->blockWrite(&ptr[0], outLen);
  } else if( mode == FCOMPARE ) {
    for( i = 0; i < outLen; i++ ) {
      uint8_t b = ptr[i];
      if( b != out->getchar() && !diffFound )
        diffFound = (int) out->curPos();
    }
  }
  return outLen;
}

inline char valueB(char c) {
  const char *p = strchr(table1, c);
  if( p )
    return (char) (p - table1);
  return 0;
}

void encodeBase64(File *in, File *out, uint64_t len64) {
  int len = (int) len64;
  int inLen = 0;
  int i = 0;
  int j = 0;
  int b = 0;
  int lfp = 0;
  int tlf = 0;
  char src[4];
  int b64Mem = (len >> 2U) * 3 + 10;
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
        src[j] = valueB(src[j]);
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
      src[j] = valueB(src[j]);

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

#endif //PAQ8PX_BASE64_HPP
