/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATBLOCK_H
#define XORSATBLOCK_H

typedef struct XORSATFilterBlock {
  uint8_t nSolutions;
  bitvector_t pSolutionsCompressed;
  XORSATFilterHash_list pHashes;
  XORSATFilterMetaData_list pMetaData;
  size_t nMetaDataBytes;
  uint32_t nVariables;
  uint8_t bBadBlock;
  uint8_t nLitsPerRow;
  uint32_t nThreadNumber;
} XORSATFilterBlock;

create_c_list_headers(XORSATFilterBlock_list, XORSATFilterBlock)

void XORSATFilterBlockFillToWord(XORSATFilterBlock *pBlock, uint8_t bIncrement);
void XORSATFilterBlockFree(XORSATFilterBlock *pBlock);

#endif
