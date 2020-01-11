#ifndef PAQ8PX_EXE_HPP
#define PAQ8PX_EXE_HPP

// EXE transform: <encoded-size> <begin> <block>...
// Encoded-size is 4 bytes, MSB first.
// begin is the offset of the start of the input file, 4 bytes, MSB first.
// Each block applies the e8e9 transform to strings falling entirely
// within the block starting from the end and working backwards.
// The 5 byte pattern is E8/E9 xx xx xx 00/FF (x86 CALL/JMP xxxxxxxx)
// where xxxxxxxx is a relative address LSB first.  The address is
// converted to an absolute address by adding the offset mod 2^25
// (in range +-2^24).

static void encodeExe(File *in, File *out, uint64_t len, uint64_t begin) {
  const int block = 0x10000;
  Array<uint8_t> blk(block);
  out->putVLI(begin); //TODO: Large file support

  // Transform
  for( uint64_t offset = 0; offset < len; offset += block ) {
    uint32_t size = min(uint32_t(len - offset), block);
    int bytesRead = (int) in->blockRead(&blk[0], size);
    if( bytesRead != (int) size )
      quit("encodeExe read error");
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

static uint64_t decodeExe(Encoder &en, uint64_t size, File *out, FMode mode, uint64_t &diffFound) {
  const int block = 0x10000; // block size
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
       (((offset - 1) ^ (offset - 6)) & -block) == 0 && offset <= (int) size ) { // not crossing block boundary
      a = ((c[1] ^ 176) | (c[2] ^ 176) << 8 | (c[3] ^ 176) << 16 | c[0] << 24) - offset - begin;
      a <<= 7U;
      a >>= 7U;
      c[3] = a;
      c[2] = a >> 8U;
      c[1] = a >> 16U;
      c[0] = a >> 24U;
    }
    if( mode == FDECOMPRESS )
      out->putChar(c[5]);
    else if( mode == FCOMPARE && c[5] != out->getchar() && !diffFound )
      diffFound = offset - 6 + 1;
    if( mode == FDECOMPRESS && !(offset & 0xfffu))
      en.printStatus();
    offset++;
  }
  return size;
}

#endif //PAQ8PX_EXE_HPP
