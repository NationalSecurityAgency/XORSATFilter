/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATHASHING_H
#define XORSATHASHING_H

#include "MurmurHash3.h"

#define XXH_INLINE_ALL
#define XXH_STATIC_LINKING_ONLY   /* access advanced declarations */
#define XXH_IMPLEMENTATION   /* access definitions */
//#include "../lib/xxHash/xxhash.h"
#include "../lib/xxHash/xxh3.h"

/*
typedef struct XORSATFilterHash {
  uint64_t h1;
  uint64_t h2:63;
  uint8_t present:1;
} XORSATFilterHash;
*/

typedef struct XORSATFilterHash128 {
  uint64_t h1;
  uint64_t h2;
} XORSATFilterHash128;

typedef struct XORSATFilterHash {
  uint64_t h1:63;
  uint8_t present:1;
} XORSATFilterHash;

typedef struct XORSATFilterRow {
  uint16_t b1;
  uint16_t b2;
  uint16_t p1;
  uint16_t p2;
  uint32_t rhs;
} XORSATFilterRow;

create_c_list_headers(XORSATFilterHash_list, XORSATFilterHash)

XORSATFilterHash XORSATFilterGenerateHashesFromElement(const void *pElement, size_t nElementBytes);
uint32_t XORSATFilterHashToBlock(XORSATFilterHash hash, uint32_t nBlocks);
void XORSATFilterGenerateRowFromHash_WRS(XORSATFilterHash xsfh, uint32_t nVariables, uint32_t *pRow, uint8_t nLitsPerRow);
XORSATFilterRow XORSATFilterGenerateRowFromHash_DW(XORSATFilterHash xsfh, uint32_t nVariables);
#endif
