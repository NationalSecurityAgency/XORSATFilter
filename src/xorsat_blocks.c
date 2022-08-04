/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

// A brief description of the parameters below is given in `include/xorsat_filter.h`.

XORSATFilterParameters XORSATFilterEfficientParameters =
  { .nLitsPerRow   = 6,
    .nSolutions    = 7,
    .nEltsPerBlock = 5000,
    .fEfficiency   = 1.00 }; //Achieved efficiency ~99%

XORSATFilterParameters XORSATFilterPaperParameters =
  { .nLitsPerRow   = 5,
    .nSolutions    = 7,
    .nEltsPerBlock = 3072,
    .fEfficiency   = 1.00 }; //Achieved efficiency ~98%

XORSATFilterParameters XORSATFilterFastParameters =
  { .nLitsPerRow   = 4,
    .nSolutions    = 7,
    .nEltsPerBlock = 750,
    .fEfficiency   = 1.00 }; //Achieved efficiency ~93%

XORSATFilterParameters XORSATFilterDWEfficientParameters =
  { .nLitsPerRow   = 2,
    .nSolutions    = 7,
    .nEltsPerBlock = 5000,
    .fEfficiency   = 1.00 }; //Achieved efficiency >99%

XORSATFilterParameters XORSATFilterDWPaperParameters =
  { .nLitsPerRow   = 2,
    .nSolutions    = 7,
    .nEltsPerBlock = 3072,
    .fEfficiency   = 1.00 }; //Achieved efficiency ~99%

XORSATFilterParameters XORSATFilterDWFastParameters =
  { .nLitsPerRow   = 2,
    .nSolutions    = 7,
    .nEltsPerBlock = 750,
    .fEfficiency   = 1.00 }; //Achieved efficiency ~95%

create_c_list_type(XORSATFilterBlock_list, XORSATFilterBlock)

void XORSATFilterBlockAlloc(XORSATFilterBlock *pBlock, uint8_t nSolutions, size_t nMetaDataBytes, uint32_t nVariablesPerBlock, uint8_t nLitsPerRow) {
  uint32_t i;

  pBlock->nSolutions = nSolutions;

  XORSATFilterHash_list_init(&pBlock->pHashes, nVariablesPerBlock);
  if(nMetaDataBytes > 0) {
    XORSATFilterMetaData_list_init(&pBlock->pMetaData, nVariablesPerBlock);
  } else {
    XORSATFilterMetaData_list_init(&pBlock->pMetaData, 0);
  }
  pBlock->nMetaDataBytes = nMetaDataBytes;
  pBlock->nVariables = nVariablesPerBlock;
  pBlock->bBadBlock = 0;
  pBlock->nLitsPerRow = nLitsPerRow;
  pBlock->nThreadNumber = 0;
}

void XORSATFilterBlockResize(XORSATFilterBlock *pBlock, uint32_t nVariablesPerBlock) {
  uint32_t i;

  //fprintf(stderr, "resizing block from %u to %u\n", pBlock->nVariables, nVariablesPerBlock);
  pBlock->nVariables = nVariablesPerBlock;
  
  XORSATFilterHash_list_resize(&pBlock->pHashes, nVariablesPerBlock);
}

void XORSATFilterBlockFillToWord(XORSATFilterBlock *pBlock, uint8_t bIncrement) {
  uint32_t i = bIncrement + (0x40 - (pBlock->nVariables + bIncrement) & 0x3f);
  XORSATFilterBlockResize(pBlock, pBlock->nVariables + i);
}

void XORSATFilterBlockFree(XORSATFilterBlock *pBlock) {
  uint32_t i;

  XORSATFilterHash_list_free(&pBlock->pHashes, NULL);
  XORSATFilterMetaData_list_free(&pBlock->pMetaData, NULL); //Don't free data, pointer is just copied.
}

uint8_t XORSATFilterDistributeHashesToBlocks(XORSATFilterBuilder *xsfb, XORSATFilterParameters sParams) {
  uint32_t i, j, k;
  uint32_t nBlocks;
  
  //Determine number of blocks
  nBlocks = (xsfb->pHashes.nLength / (uint32_t) sParams.nEltsPerBlock);

#ifdef XORSATFILTER_PRINT_BUILD_PROGRESS
  fprintf(stderr, "%u blocks, roughly %u variables per block\n", nBlocks, sParams.nEltsPerBlock);
#endif
  
  //Initialize blocks
  uint8_t ret = XORSATFilterBlock_list_resize(&xsfb->pBlocks, nBlocks);
  if(ret != C_LIST_NO_ERROR) return ret;
  
  xsfb->pBlocks.nLength = nBlocks;
  for(i = 0; i < nBlocks; i++) {
    XORSATFilterBlockAlloc(&xsfb->pBlocks.pList[i], sParams.nSolutions, xsfb->nMetaDataBytes, sParams.nEltsPerBlock, sParams.nLitsPerRow);
    if(xsfb->pBlocks.pList[i].bBadBlock) return 1;
  }
  
  //Distribute elements over blocks
  for(i = 0; i < xsfb->pHashes.nLength; i++) {
    XORSATFilterHash pHash = xsfb->pHashes.pList[i];
    uint32_t nBlock = XORSATFilterHashToBlock(pHash, nBlocks);
    XORSATFilterBlock *pBlock = &xsfb->pBlocks.pList[nBlock];
    ret = XORSATFilterHash_list_push(&pBlock->pHashes, pHash);
    if(ret != C_LIST_NO_ERROR) return ret;

    if(xsfb->nMetaDataBytes > 0) {
      XORSATFilterMetaData pMetaData = xsfb->pMetaData.pList[i];
      ret = XORSATFilterMetaData_list_push(&pBlock->pMetaData, pMetaData);
      if(ret != C_LIST_NO_ERROR) return ret;
    }
  }

  XORSATFilterHash_list_free(&xsfb->pHashes, NULL);

  //Determine approximate number of variables to use for each block
  for(i = 0; i < nBlocks; i++) {
    XORSATFilterBlock *pBlock = &xsfb->pBlocks.pList[i];
    XORSATFilterBlockResize(pBlock, (1.0 / sParams.fEfficiency) * (float) pBlock->pHashes.nLength);
    XORSATFilterBlockFillToWord(pBlock, 0);
  }

  return 0;
}
