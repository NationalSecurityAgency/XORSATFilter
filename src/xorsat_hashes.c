/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

create_c_list_type(XORSATFilterHash_list, XORSATFilterHash)

//0 for xxHash
//1 for MurmurHash3
#define MURMURHASH 0

inline
XORSATFilterHash XORSATFilterGenerateHashesFromElement(const void *pElement, size_t nElementBytes) {
  XORSATFilterHash xsfh;
  uint64_t nonce = 0;
  assert(nElementBytes < 0x7fffffff);

  do {  
    if(MURMURHASH) {
      uint64_t murmurhash[2];
      MurmurHash3_x86_128(pElement, (int)nElementBytes, (uint64_t)0x1ae202980e70d8f1 + (nonce++), (void *)murmurhash);
      xsfh.h1 = murmurhash[0];
      //xsfh.h2 = murmurhash[1];
    } else {
      xsfh.h1 = XXH3_64bits_withSeed(pElement, nElementBytes, (unsigned long long)0x1ae202980e70d8f1 + (nonce++));
      //XXH128_hash_t hash = XXH3_128bits_withSeed(pElement, nElementBytes, (unsigned long long)0x1ae202980e70d8f1 + (nonce++));
      //xsfh.h1 = hash.low64;
      //xsfh.h2 = hash.high64
      //xsfh.h1 = XXH64(pElement, nElementBytes, (unsigned long long)0x1ae202980e70d8f1 + (nonce++));
      //xsfh.h2 = XXH64(pElement, nElementBytes, (unsigned long long)xsfh.h1);
      //xsfh.h2 = xsfh.h1 ^ 0xc93bd65d1f9ade1a; //Slightly faster
    }
  } while (xsfh.h1 == 0);

  xsfh.present = 1;
  
  return xsfh;
}

inline
uint32_t XORSATFilterHashToBlock(XORSATFilterHash xsfh, uint32_t nBlocks) {
  return ((uint32_t *)&xsfh)[0] % nBlocks;
}

inline
void XORSATFilterGenerateRowFromHash_WRS(XORSATFilterHash xsfh, uint32_t nVariables, uint32_t *pRow, uint8_t nLitsPerRow) {
  uint32_t i = 0;

  XORSATFilterHash128 xsfh_128;
  xsfh_128.h1 = xsfh.h1;
  xsfh_128.h2 = xsfh_128.h1 ^ 0xc93bd65d1f9ade1a;
  
  uint16_t *xsfh_16 = xsfh_128.h16;

  //Can get 5 values with little effort
  for(; i < nLitsPerRow && i < 5; i++) {
    pRow[i] = xsfh_16[i] % nVariables;
  }

  //Spin up an LFSR for larger number of literals
  for(; i < nLitsPerRow; i++) {
    //Primitive polynomial 1 + x^2 + x^3 + x^4 + x^8
    xsfh_128.h1 = xsfh_128.h1 >> 16;
    xsfh_16[3]  = xsfh_16[4];
    xsfh_128.h2 = xsfh_128.h2 >> 16;
    xsfh_16[7] ^= xsfh_16[0] ^ xsfh_16[2] ^ xsfh_16[3] ^ xsfh_16[4];

    pRow[i] = xsfh_16[5] % nVariables;
  }

  pRow[i] = xsfh.present ? ((uint32_t *)&xsfh_128)[3] : ~((uint32_t *)&xsfh_128)[3]; //Allow up to 32 solutions
}

inline
XORSATFilterRow XORSATFilterGenerateRowFromHash_DW(XORSATFilterHash xsfh, uint32_t nVariables) {
  XORSATFilterHash128 xsfh_128;
      
  xsfh_128.h1 = xsfh.h1;
  //xsfh_128.h2 = XXH64((uint8_t *)&xsfh_128.h1, 4, (unsigned long long)0x1ae202980e70d8f1);
  //xsfh_128.h2 = XXH64(NULL, 0, xsfh_128.h1);
  xsfh_128.h2 = xsfh.h1 ^ 0xc93bd65d1f9ade1a;

  uint16_t *xsfh_16 = xsfh_128.h16;
  uint32_t *xsfh_32 = xsfh_128.h32;

  uint32_t nBlocks = nVariables >> 4;
  
  XORSATFilterRow xsfrow;
  xsfrow.b1 = xsfh_16[2] % nBlocks;
  xsfrow.b2 = ((xsfh_16[3] % (nBlocks - 1)) + 1 + xsfrow.b1) % nBlocks;

  xsfrow.p1 = xsfh_16[4];
  if(xsfrow.p1 == 0) xsfrow.p1 = 1;
  xsfrow.p2 = xsfh_16[5];
  if(xsfrow.p2 == 0) xsfrow.p2 = 1;
  
  xsfrow.rhs = xsfh.present ? xsfh_32[3] : ~xsfh_32[3]; //Allow up to 32 solutions
  return xsfrow;
}
