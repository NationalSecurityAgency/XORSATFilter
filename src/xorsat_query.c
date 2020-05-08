/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

XORSATFilterQuerier *XORSATFilterQuerierAlloc(uint32_t nFilterWords, uint32_t nBlocks, uint16_t nAvgVarsPerBlock, uint8_t nSolutions, size_t nMetaDataBytes, uint8_t nLitsPerRow) {
  XORSATFilterQuerier *xsfq = (XORSATFilterQuerier *)malloc(1 * sizeof(XORSATFilterQuerier));
  if(xsfq == NULL) return NULL;
  
  xsfq->pFilter = (uint64_t *)malloc(nFilterWords * sizeof(uint64_t));
  if(xsfq->pFilter == NULL) {
    free(xsfq);
    return NULL;
  }

  xsfq->pOffsets = (int16_t *)malloc((nBlocks+1) * sizeof(int16_t));
  if(xsfq->pOffsets == NULL) {
    free(xsfq->pFilter);
    free(xsfq);
    return NULL;
  }

  xsfq->nBlocks = nBlocks;
  xsfq->nAvgVarsPerBlock = nAvgVarsPerBlock;
  xsfq->nSolutions = nSolutions;
  xsfq->nMetaDataBytes = nMetaDataBytes;
  xsfq->nLitsPerRow = nLitsPerRow;
  xsfq->bMMAP = 0;

  return xsfq;
}

void XORSATFilterQuerierFree(XORSATFilterQuerier *xsfq) {
  if(xsfq->bMMAP) {
    if(xsfq->pFilter != NULL) {
      uint64_t nFilterWords = XORSATFilterGetBlockIndex(xsfq, xsfq->nBlocks);
      munmap(xsfq->pFilter, (nFilterWords * sizeof(uint64_t)) + (((uint64_t) xsfq->nBlocks+1) * sizeof(int16_t)));
    }
    xsfq->bMMAP = 0;
  } else {
    free(xsfq->pFilter);
    free(xsfq->pOffsets);
  }

  free(xsfq);
}

uint8_t XORSATFilterStoreBlockIndex(XORSATFilterQuerier *xsfq, uint32_t nBlock, uint64_t nBlockIndex) {
  int64_t nExpectedIndex = (uint64_t) xsfq->nAvgVarsPerBlock * nBlock;
  //Round up to next multiple of 64
  nExpectedIndex = ((nExpectedIndex-1) | 0x3f) + 1;
  int64_t nDiff = (nExpectedIndex - ((int64_t) nBlockIndex)) >> 6;
  if(nDiff <= 32767 && nDiff >= -32768) {
    xsfq->pOffsets[nBlock] = (int16_t) nDiff;
    return 1;
  } else {
    fprintf(stderr, "Filter is too large. Decrease number of elements or try making XORSATFilterParameters.nEltsPerBlock larger.\n");
    return 0;
  }
}

inline
uint64_t XORSATFilterGetBlockIndex(XORSATFilterQuerier *xsfq, uint32_t nBlock) {
  int64_t nDiff = (int64_t) xsfq->pOffsets[nBlock];
  int64_t nExpectedIndex = ((int64_t) xsfq->nAvgVarsPerBlock) * (int64_t) nBlock;
  //Round up to next multiple of 64
  nExpectedIndex = ((nExpectedIndex-1) | (int64_t) 0x3f) + (int64_t) 1;
  return ((nExpectedIndex - (nDiff * 64)) >> 6) * (uint64_t) (xsfq->nSolutions + (xsfq->nMetaDataBytes*8));
}

void XORSATFilterStoreBlockSolution_WRS(XORSATFilterQuerier *xsfq, XORSATFilterBlock *pBlock, uint32_t nBlockIndex) {
  uint32_t i, j;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  uint64_t nWord = 0;
  uint64_t nBit = 0;
  uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, nBlockIndex);
  uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, nBlockIndex+1) - nBlockStart;
  uint32_t nVariables = (nBlockSize / nRHSBits) << 6;
  
  if(pBlock->bBadBlock) {
    for(i = 0; i < nBlockSize; i++) {
      xsfq->pFilter[nBlockStart++] = 0;
    }
  } else {
    for(i = 0; i < nVariables; i++) {
      for(j = 0; j < nRHSBits; j++) {
	nWord >>= 1;
	nWord |= bitvector_t_getBit(&pBlock->pSolutionsCompressed, (i*nRHSBits) + j) ? 0x8000000000000000 : 0x0;
	nBit++;
	if((nBit & 0x3f) == 0) {
	  xsfq->pFilter[nBlockStart++] = nWord;
	  nWord = 0;
	}
      }
    }
  }
  uint64_t_list_free(&pBlock->pSolutionsCompressed.bits, NULL);
}

void XORSATFilterStoreBlockSolution_DW(XORSATFilterQuerier *xsfq, XORSATFilterBlock *pBlock, uint32_t nBlockIndex) {
  uint32_t i, j;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  uint64_t nWord = 0;
  uint64_t nBit = 0;
  uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, nBlockIndex);
  uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, nBlockIndex+1) - nBlockStart;
  uint32_t nVariables = (nBlockSize / nRHSBits) << 6;
  
  if(pBlock->bBadBlock) {
    for(i = 0; i < nBlockSize; i++) {
      xsfq->pFilter[nBlockStart++] = 0;
    }
  } else {
    for(j = 0; j < nRHSBits; j++) {
      for(i = 0; i < nVariables; i++) {
	nWord >>= 1;
	nWord |= bitvector_t_getBit(&pBlock->pSolutionsCompressed, (i*nRHSBits) + j) ? 0x8000000000000000 : 0x0;
	nBit++;
	if((nBit & 0x3f) == 0) {
	  xsfq->pFilter[nBlockStart++] = nWord;
	  nWord = 0;
	}
      }
    }
  }
  uint64_t_list_free(&pBlock->pSolutionsCompressed.bits, NULL);
}

XORSATFilterQuerier *XORSATFilterCreateQuerierFromBuilder(XORSATFilterBuilder *xsfb) {
  uint32_t i;
  uint32_t nBlocks = xsfb->pBlocks.nLength;
  uint64_t nRHSBits = ((uint32_t) xsfb->pBlocks.pList[0].nSolutions) + (xsfb->nMetaDataBytes*8);

  uint64_t nFilterBits = 0;
  uint64_t nAvgVarsPerBlock = 0;
  for(i = 0; i < nBlocks; i++) {
    nFilterBits += (uint64_t) xsfb->pBlocks.pList[i].nVariables * nRHSBits;
    nAvgVarsPerBlock += xsfb->pBlocks.pList[i].nVariables;
    assert(nFilterBits % 64 == 0);
  }
  nAvgVarsPerBlock /= (uint64_t) xsfb->pBlocks.nLength;
 
  uint32_t nFilterWords = (uint32_t) (nFilterBits >> 6);
  
  XORSATFilterQuerier *xsfq = XORSATFilterQuerierAlloc(nFilterWords, nBlocks, nAvgVarsPerBlock, xsfb->pBlocks.pList[0].nSolutions, xsfb->nMetaDataBytes, xsfb->pBlocks.pList[0].nLitsPerRow);
  if(xsfq == NULL) return NULL;
  
  uint64_t nBlockIndex = 0;
  for(i = 0; i < nBlocks; i++) {
    uint8_t ret = XORSATFilterStoreBlockIndex(xsfq, i, nBlockIndex);
    if(ret != 1) return NULL;
    nBlockIndex += (uint64_t) xsfb->pBlocks.pList[i].nVariables;
  }
  XORSATFilterStoreBlockIndex(xsfq, i, nBlockIndex);
  
  //Store transposed solution
  for(i = 0; i < nBlocks; i++) {
    if(xsfq->nLitsPerRow < 3) {
      XORSATFilterStoreBlockSolution_DW(xsfq, &xsfb->pBlocks.pList[i], i);
    } else {
      XORSATFilterStoreBlockSolution_WRS(xsfq, &xsfb->pBlocks.pList[i], i);
    }
  }

  return xsfq;
}

uint8_t XORSATFilterQueryBlock_WRS(XORSATFilterQuerier *xsfq, uint32_t nVariables, XORSATFilterHash pHash, uint64_t *pFilterBlock) {
  uint32_t i, j;
  uint32_t pRow[xsfq->nLitsPerRow + 1];

  //Generate row
  XORSATFilterGenerateRowFromHash_WRS(pHash, nVariables, pRow, xsfq->nLitsPerRow);

  //compare row to pfilterblock
  uint32_t nSolutions = xsfq->nSolutions;
  if(nSolutions == 0) return 1;

  size_t nMetaDataBits = xsfq->nMetaDataBytes * 8;
  uint32_t nRHSBits = nSolutions + nMetaDataBits;
  uint32_t length = (nSolutions+63) >> 6;
  uint64_t passed[length];
  for(i = 0; i < length; i++) {
    passed[i] = pRow[xsfq->nLitsPerRow]; //For nSolutions <= 32.
  }
  
  for(j = 0; j < xsfq->nLitsPerRow; j++) {
    uint32_t var = pRow[j];
    uint32_t bit = var * nRHSBits;
    
    for(i = 0; i < length; i++) {
      uint64_t chunk_f = pFilterBlock[(bit>>6) + i] >> (bit & 0x3f);
      if(i < (((bit + nSolutions)>>6) - (bit >> 6)) - (((bit+nSolutions) & 0x3f) == 0)) {
        if((bit & 0x3f) != 0) { //X86 shifts are mod 64
          chunk_f |= pFilterBlock[(bit>>6) + i + 1] << (64 - (bit & 0x3f));
        }
      } else {
        //last chunk
      }
      passed[i] ^= chunk_f;
    }
  }
  
  passed[length-1] &= (~(uint64_t)0) >> (64 - (nSolutions & 0x3f));
  
  for(i = 0; i < length; i++) {
    if(passed[i] != 0) {
      //fprintf(stdout, "No\n");
      return 0;
    }
  }
  
  //fprintf(stdout, "Maybe\n");
  return 1;
}

uint8_t *XORSATFilterRetrieveMetadataBlock_WRS(XORSATFilterQuerier *xsfq, uint32_t nVariables, XORSATFilterHash pHash, uint64_t *pFilterBlock) {
  uint32_t i;
  uint32_t pRow[xsfq->nLitsPerRow + 1];

  //Generate row
  XORSATFilterGenerateRowFromHash_WRS(pHash, nVariables, pRow, xsfq->nLitsPerRow);

  //compare row to pfilterblock
  uint32_t nSolutions = xsfq->nSolutions;
  size_t nMetaDataBits = xsfq->nMetaDataBytes * 8;
  uint32_t nRHSBits = nSolutions + nMetaDataBits;
  size_t nLength = ((nVariables * nRHSBits)+63) >> 6;

  uint64_t *pMetaData = (uint64_t *)calloc((xsfq->nMetaDataBytes >> 2) + 1, sizeof(uint64_t));

  for(i = 0; i < xsfq->nLitsPerRow; i++) {
    uint32_t var = pRow[i];

    size_t start = (var * nRHSBits) + nSolutions;
    size_t startW  = start >> 6;
    size_t lengthW = (nMetaDataBits+63) >> 6;
    size_t j;
    for(j = 0; j < lengthW; j++) {
      pMetaData[j] ^= pFilterBlock[startW + j] >> (start&0x3f);
      if((startW + j + 1 < nLength) && ((start&0x3f) != 0)) {
        pMetaData[j] ^= pFilterBlock[startW + j + 1] << (64 - (start&0x3f));
      }
    }
  }

  return (uint8_t *)pMetaData;
}

uint8_t XORSATFilterQueryBlock_DW(XORSATFilterQuerier *xsfq, uint32_t nVariables, XORSATFilterHash pHash, uint64_t *pFilterBlock) {
  uint32_t i;

  //compare row to pfilterblock
  uint32_t nSolutions = xsfq->nSolutions;
  if(nSolutions == 0) return 1;

  size_t nMetaDataBits = xsfq->nMetaDataBytes * 8;
  uint32_t nRHSBits = nSolutions + nMetaDataBits;
  
  //Generate row
  XORSATFilterRow xsfrow = XORSATFilterGenerateRowFromHash_DW(pHash, nVariables);

  uint32_t nPassed = 0;
  for(i = 0; i < nSolutions; i++) {
    size_t start1 = (i * nVariables) + (xsfrow.b1 * 16);
    size_t start1W  = start1 >> 6;
    uint16_t chunk_1 = (pFilterBlock[start1W] >> (start1 & 0x3f)) & xsfrow.p1;

    size_t start2 = (i * nVariables) + (xsfrow.b2 * 16);
    size_t start2W  = start2 >> 6;
    uint16_t chunk_2 = (pFilterBlock[start2W] >> (start2 & 0x3f)) & xsfrow.p2;

    uint32_t chunk_f = ((uint32_t) chunk_2) ^ (((uint32_t) chunk_1) << 16);

    if((xsfrow.rhs & 0x1) != __builtin_parity(chunk_f)) {
      //fprintf(stdout, "No\n");
      return 0;
    }

    xsfrow.rhs >>= 1;
  }

  //fprintf(stdout, "Maybe\n");
  return 1;
}

uint8_t *XORSATFilterRetrieveMetadataBlock_DW(XORSATFilterQuerier *xsfq, uint32_t nVariables, XORSATFilterHash pHash, uint64_t *pFilterBlock) {
  uint32_t i;

  uint32_t nSolutions = xsfq->nSolutions;
  size_t nMetaDataBits = xsfq->nMetaDataBytes * 8;
  uint32_t nRHSBits = nSolutions + nMetaDataBits;
  
  //Generate row
  XORSATFilterRow xsfrow = XORSATFilterGenerateRowFromHash_DW(pHash, nVariables);
  
  uint8_t *pMetaData = (uint8_t *)calloc(xsfq->nMetaDataBytes, sizeof(uint8_t));

  size_t nByte = 0;
  size_t nBit = 0;
  for(i = nSolutions; i < nRHSBits; i++) {
    size_t start1 = (i * nVariables) + (xsfrow.b1 * 16);
    size_t start1W  = start1 >> 6;
    uint16_t chunk_1 = (pFilterBlock[start1W] >> (start1 & 0x3f)) & xsfrow.p1;

    size_t start2 = (i * nVariables) + (xsfrow.b2 * 16);
    size_t start2W  = start2 >> 6;
    uint16_t chunk_2 = (pFilterBlock[start2W] >> (start2 & 0x3f)) & xsfrow.p2;

    uint32_t chunk_f = ((uint32_t) chunk_2) ^ (((uint32_t) chunk_1) << 16);

    pMetaData[nByte] >>= 1;
    pMetaData[nByte] ^= __builtin_parity(chunk_f) ? 0x80 : 0x00;
    nBit++;
    if((nBit & 0x7) == 0) {
      nByte++;
    }
  }

  return (uint8_t *)pMetaData;
}

inline
uint8_t XORSATFilterQuery(XORSATFilterQuerier *xsfq, const void *pElement, uint32_t nElementBytes) {
  uint8_t bPass;
  
  //Generate hashes from element
  XORSATFilterHash pHash = XORSATFilterGenerateHashesFromElement(pElement, nElementBytes);
  
  //Hash to block
  uint32_t nBlockIndex = XORSATFilterHashToBlock(pHash, xsfq->nBlocks);
  //Get filter block

  uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, nBlockIndex);
  uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, nBlockIndex+1) - nBlockStart;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  uint32_t nVariables = (nBlockSize / nRHSBits) << 6;

  uint64_t *pFilterBlock = xsfq->pFilter + nBlockStart;

  if(pFilterBlock[0] == 0 || nVariables == 0) {
    bPass = 1; //Bad Block
  } else {
    //Query filter block
    if(xsfq->nLitsPerRow < 3) {
      bPass = XORSATFilterQueryBlock_DW(xsfq, nVariables, pHash, pFilterBlock);
    } else {
      bPass = XORSATFilterQueryBlock_WRS(xsfq, nVariables, pHash, pFilterBlock);
    }
  }
  
  return bPass;
}

uint8_t *XORSATFilterRetrieveMetadata(XORSATFilterQuerier *xsfq, const void *pElement, uint32_t nElementBytes) {
  uint8_t *bPass;
  
  //Generate hashes from element
  XORSATFilterHash pHash = XORSATFilterGenerateHashesFromElement(pElement, nElementBytes);
  
  //Hash to block
  uint32_t nBlockIndex = XORSATFilterHashToBlock(pHash, xsfq->nBlocks);
  //Get filter block

  uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, nBlockIndex);
  uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, nBlockIndex+1) - nBlockStart;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  uint32_t nVariables = (nBlockSize / nRHSBits) << 6;

  uint64_t *pFilterBlock = xsfq->pFilter + nBlockStart;

  if(pFilterBlock[0] == 0 || nVariables == 0 || xsfq->nMetaDataBytes == 0) {
    bPass = NULL; //Bad Block
  } else {
    //Query filter block
    if(xsfq->nLitsPerRow < 3) {
      bPass = XORSATFilterRetrieveMetadataBlock_DW(xsfq, nVariables, pHash, pFilterBlock);
    } else {
      bPass = XORSATFilterRetrieveMetadataBlock_WRS(xsfq, nVariables, pHash, pFilterBlock);
    }
  }
  
  return bPass;
}

/*************************************************************************************

  Utility functions for computing statistics about k-XORSAT set-membership filters.

**************************************************************************************/

uint32_t XORSATFilterQueryRate(XORSATFilterQuerier *xsfq) {
  uint32_t i, j;
  uint32_t nElementsQueried = 10000000;

  clock_t start = clock();
  for(i = 0; i < nElementsQueried; i++) {
    uint8_t volatile ret = XORSATFilterQuery(xsfq, &i, sizeof(uint32_t));
  }
  clock_t end = clock();
  
  double time_elapsed_in_seconds = ((double) (end - start)) / (double) CLOCKS_PER_SEC;
  return (uint32_t) (((double) nElementsQueried) / time_elapsed_in_seconds);
}

uint32_t XORSATFilterMetadataRetrievalRate(XORSATFilterQuerier *xsfq, uint32_t nElementBytes) {
  uint32_t i, j;
  uint32_t nElementsQueried = 10000000;

  clock_t start = clock();
  for(i = 0; i < nElementsQueried; i++) {
    uint8_t * volatile ret = XORSATFilterRetrieveMetadata(xsfq, &i, nElementBytes);
    free(ret);
  }
  clock_t end = clock();
  
  double time_elapsed_in_seconds = ((double) (end - start)) / (double) CLOCKS_PER_SEC;
  return (uint32_t) (((double) nElementsQueried) / time_elapsed_in_seconds);
}

double XORSATFilterFalsePositiveRate(XORSATFilterQuerier *xsfq) {
  uint32_t nNoes = 0;
  uint32_t i = 0;
  uint32_t nElementBytes = sizeof(uint32_t);
  double p;

  do {
    uint8_t ret = XORSATFilterQuery(xsfq, &i, nElementBytes);
    if(ret == 0) nNoes++;
    p = 1.0 - ((double) nNoes) / (double) (i+1);
  } while((i != ~0) && ((sqrt((p*(1-p)) / ((double) ++i)) > (p/100.0)) || (nNoes >= i-1)));

  return p;
}

uint64_t XORSATAncillarySize(XORSATFilterQuerier *xsfq) {
  uint64_t i;
  uint64_t nAncillaryBits = 0;
  nAncillaryBits += ((uint64_t) xsfq->nBlocks + 1) * (uint64_t) 16;
  nAncillaryBits += sizeof(XORSATFilterSerialData) * (uint64_t) 8;

  return nAncillaryBits;
}

uint64_t XORSATFilterSize(XORSATFilterQuerier *xsfq) {
  uint64_t i;
  uint64_t nFilterBits = 0;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  for(i = 0; i < xsfq->nBlocks; i++) {
    uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, i);
    uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, i+1) - nBlockStart; 
    uint32_t nVariables = (nBlockSize / nRHSBits) << 6;
    nFilterBits += nVariables * (uint64_t) xsfq->nSolutions;
  }

  return nFilterBits;
}

uint64_t XORSATMetaDataSize(XORSATFilterQuerier *xsfq) {
  uint64_t i;
  uint64_t nMetaDataBits = 0;
  uint32_t nRHSBits = ((uint32_t) xsfq->nSolutions) + (xsfq->nMetaDataBytes*8);
  for(i = 0; i < xsfq->nBlocks; i++) {
    uint64_t nBlockStart = XORSATFilterGetBlockIndex(xsfq, i);
    uint32_t nBlockSize  = XORSATFilterGetBlockIndex(xsfq, i+1) - nBlockStart; 
    uint32_t nVariables = (nBlockSize / nRHSBits) << 6;
    nMetaDataBits += nVariables * ((uint64_t) xsfq->nMetaDataBytes) * 8;
  }

  return nMetaDataBits;
}

double XORSATFilterEfficiency(XORSATFilterQuerier *xsfq, uint64_t nElements, double p) {
  if(p == 0.0) {
    p = XORSATFilterFalsePositiveRate(xsfq);
  }

  uint64_t nFilterBits = XORSATFilterSize(xsfq);
  nFilterBits += XORSATAncillarySize(xsfq);

  return (-(log(p) / log(2))) * (((double)nElements) / ((double)nFilterBits));
}

double XORSATMetaDataEfficiency(XORSATFilterQuerier *xsfq, uint64_t nElements) {
  uint64_t nMetaDataBits = XORSATMetaDataSize(xsfq);
  nMetaDataBits += XORSATAncillarySize(xsfq);

  return (((double)nElements) * ((double)xsfq->nMetaDataBytes*8)) / (double) nMetaDataBits;
}
