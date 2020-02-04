/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

uint8_t XORSATFilterSolveBlock(XORSATFilterBlock *pBlock) {
  uint8_t ret = 1;
  uint32_t i, j;

  //Remove duplicate hashes
  uint8_t duplicate_message_printed = 0;
  if(pBlock->pHashes.nLength > 1) {
    for(i = 0; i < pBlock->pHashes.nLength-1; i++) {
      XORSATFilterHash xsfh = pBlock->pHashes.pList[i];
      for(j = i+1; j < pBlock->pHashes.nLength; j++) {
	if(xsfh.h1 == pBlock->pHashes.pList[j].h1) {
	  //Removing duplicate
	  pBlock->pHashes.pList[j] = pBlock->pHashes.pList[--pBlock->pHashes.nLength];
	  j--;
	  if(duplicate_message_printed == 0) {
	    fprintf(stderr, "Hash collision or duplicate element detected. Possible loss of data. Consider using a better hash function\n");
	    duplicate_message_printed = 1;
	  }
	}
      }
    }
  }
  
  while (1) {
    gf2_t *pMatrix;
    if(pBlock->nLitsPerRow < 3) {
      pMatrix = XORSATFilterBuildIMMIRMatrix_DW(pBlock);
    } else {
      pMatrix = XORSATFilterBuildIMMIRMatrix_WRS(pBlock);
    }

    //If unsat, mark as bad block and return
    if(ret == 0) {
      pBlock->bBadBlock = 1;
    } else {
      //find pBlock->nSolutions solutions
      bitvector_t *pSolutions = (bitvector_t *)malloc(pBlock->nVariables * sizeof(bitvector_t));
      if(pSolutions == NULL) {
	pBlock->bBadBlock = 1;
	gf2_clear(pMatrix); free(pMatrix);
	return 0;
      }
      for(i = 0; i < pBlock->nVariables; i++) {
	bitvector_t *pBitVector = bitvector_t_alloc(pBlock->nSolutions + (pBlock->nMetaDataBytes * 8)); //Vectors are zeroized
	if(pBitVector == NULL)  {
	  pBlock->bBadBlock = 1;
	  for(; i != 0; i--) uint64_t_list_free(&pSolutions[i-1].bits, NULL);
	  free(pSolutions);
	  gf2_clear(pMatrix); free(pMatrix);
	  return 0;
	}
	pSolutions[i] = *pBitVector;
	free(pBitVector);
      }
      
      ret = XORSATFilterFindIMMIRSolutions(pMatrix, pSolutions);

      pBlock->pSolutionsCompressed = *bitvector_t_alloc(pBlock->nVariables * (pBlock->nSolutions + (pBlock->nMetaDataBytes * 8)));
      for(i = 0; i < pBlock->nVariables; i++) {
	for(j = 0; j < pBlock->nSolutions + (pBlock->nMetaDataBytes * 8); j++) {
	  uint8_t bit = bitvector_t_getBit(&pSolutions[i], j);
	  bitvector_t_setBit(&pBlock->pSolutionsCompressed, i*(pBlock->nSolutions + (pBlock->nMetaDataBytes*8)) + j, bit);
	}
      }
      
      for(i = 0; i < pBlock->nVariables; i++) {
	uint64_t_list_free(&pSolutions[i].bits, NULL);
      }
      free(pSolutions);
      
      if(ret != 1) {
        pBlock->bBadBlock = 1; //UNSAT
      }
    }

    gf2_clear(pMatrix); free(pMatrix);
    
    if(pBlock->bBadBlock) {
      ret = 1;
      pBlock->bBadBlock = 0;
      XORSATFilterBlockFillToWord(pBlock, 1);
      //fprintf(stderr, "%u\n", pBlock->nVariables);
    } else {
      //fprintf(stderr, "\nFinished %u\n", pBlock->nVariables);
      break;
    }
  }
  
  XORSATFilterHash_list_free(&pBlock->pHashes, NULL);
  XORSATFilterMetaData_list_free(&pBlock->pMetaData, NULL); //Don't free data, pointer is just copied.
  
  return 0;
}
