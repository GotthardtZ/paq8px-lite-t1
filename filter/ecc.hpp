#ifndef PAQ8PX_ECC_HPP
#define PAQ8PX_ECC_HPP

// Function eccCompute(), edcCompute() and eccedcInit() taken from
// ** UNECM - Decoder for ECM (Error code Modeler) format.
// ** version 1.0
// ** Copyright (c) 2002 Neill Corlett

/* LUTs used for computing ECC/EDC */
static uint8_t eccFLut[256];
static uint8_t eccBLut[256];
static uint32_t edcLut[256];
static int lutsInit = 0;

void eccedcInit() {
  if( lutsInit )
    return;
  uint32_t i, j, edc;
  for( i = 0; i < 256; i++ ) {
    j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
    eccFLut[i] = j;
    eccBLut[i ^ j] = i;
    edc = i;
    for( j = 0; j < 8; j++ )
      edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
    edcLut[i] = edc;
  }
  lutsInit = 1;
}

void eccCompute(const uint8_t *src, uint32_t majorCount, uint32_t minorCount, uint32_t majorMult, uint32_t minorInc, uint8_t *dest) {
  uint32_t size = majorCount * minorCount;
  uint32_t major, minor;
  for( major = 0; major < majorCount; major++ ) {
    uint32_t index = (major >> 1U) * majorMult + (major & 1U);
    uint8_t eccA = 0;
    uint8_t eccB = 0;
    for( minor = 0; minor < minorCount; minor++ ) {
      uint8_t temp = src[index];
      index += minorInc;
      if( index >= size )
        index -= size;
      eccA ^= temp;
      eccB ^= temp;
      eccA = eccFLut[eccA];
    }
    eccA = eccBLut[eccFLut[eccA] ^ eccB];
    dest[major] = eccA;
    dest[major + majorCount] = eccA ^ eccB;
  }
}

uint32_t edcCompute(const uint8_t *src, int size) {
  uint32_t edc = 0;
  while( size-- )
    edc = (edc >> 8U) ^ edcLut[(edc ^ (*src++)) & 0xFFU];
  return edc;
}

#endif //PAQ8PX_ECC_HPP
