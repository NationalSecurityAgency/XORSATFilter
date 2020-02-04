/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

uint8_t immir_low_bit[256] = {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

gf2_t *XORSATFilterBuildIMMIRMatrix_WRS(XORSATFilterBlock *pBlock) {
  uint32_t i;
  size_t j;
  uint32_t pRow[pBlock->nLitsPerRow + 1];

  //Allocate matrix
  gf2_t *pMatrix = calloc(1, sizeof(gf2_t));
  if(pMatrix == NULL) return NULL;
  
  pMatrix->m = pBlock->pHashes.nLength;
  pMatrix->n = pBlock->nVariables;
  pMatrix->b = pBlock->nSolutions + (pBlock->nMetaDataBytes * 8);
  gf2_init(pMatrix);
  
  //Add rows
  for(i = 0; i < pBlock->pHashes.nLength; i++) {
    XORSATFilterGenerateRowFromHash_WRS(pBlock->pHashes.pList[i], pBlock->nVariables, pRow, pBlock->nLitsPerRow);
    
    //Add variables
    for(j = 0; j < pBlock->nLitsPerRow; j++) {
      uint32_t word = pRow[j]/64;
      uint64_t bit = pRow[j]%64;
      ((uint64_t *)pMatrix->matrix)[i*pMatrix->wds + word] ^= (((uint64_t)1) << bit);
    }
    
    //Add solution bits
    for(j = 0; j < pBlock->nSolutions; j++) {
      if((pRow[pBlock->nLitsPerRow]>>j) & 1) {
        uint32_t word = (pBlock->nVariables+j)/64;
        uint64_t bit = (pBlock->nVariables+j)%64;
        ((uint64_t *)pMatrix->matrix)[i*pMatrix->wds + word] |= (((uint64_t)1) << bit);
      }
    }

    if(pBlock->nMetaDataBytes > 0) {
      //Add meta data bits
      uint8_t *pMetaData = (pBlock->pMetaData.pList[i]).pMetaData;
      size_t nMetaDataByte = 0;
      uint8_t nMetaDataBit = 0;
      for(; j < pMatrix->b; j++) {
        if((pMetaData[nMetaDataByte] >> nMetaDataBit) & 1) {
          uint32_t word = (pBlock->nVariables+j)/64;
          uint64_t bit = (pBlock->nVariables+j)%64;
          ((uint64_t *)pMatrix->matrix)[i*pMatrix->wds + word] |= (((uint64_t)1) << bit);
        }
        if(++nMetaDataBit == 8) {
          nMetaDataBit = 0;
          nMetaDataByte++;
        }
      }
    }
  }
  
  return pMatrix;
}

gf2_t *XORSATFilterBuildIMMIRMatrix_DW(XORSATFilterBlock *pBlock) {
  uint32_t i;
  size_t j;

  //Allocate matrix
  gf2_t *pMatrix = calloc(1, sizeof(gf2_t));
  if(pMatrix == NULL) return NULL;

  pMatrix->m = pBlock->pHashes.nLength;
  pMatrix->n = pBlock->nVariables;
  pMatrix->b = pBlock->nSolutions + (pBlock->nMetaDataBytes * 8);
  gf2_init(pMatrix);

  //Add rows
  for(i = 0; i < pBlock->pHashes.nLength; i++) {
    XORSATFilterRow xsfrow = XORSATFilterGenerateRowFromHash_DW(pBlock->pHashes.pList[i], pBlock->nVariables);

    //Add variables
    ((uint16_t *)pMatrix->matrix)[i*4*pMatrix->wds + xsfrow.b1] ^= xsfrow.p1;
    ((uint16_t *)pMatrix->matrix)[i*4*pMatrix->wds + xsfrow.b2] ^= xsfrow.p2;

    //Add solution bits
    for(j = 0; j < pBlock->nSolutions; j++) {
      if((xsfrow.rhs>>j) & 1) {
	uint32_t word = (pBlock->nVariables+j)/64;
	uint64_t bit = (pBlock->nVariables+j)%64;
	((uint64_t *)pMatrix->matrix)[i*pMatrix->wds + word] |= (((uint64_t)1) << bit);
      }
    }

    if(pBlock->nMetaDataBytes > 0) {
      //Add meta data bits
      uint8_t *pMetaData = (pBlock->pMetaData.pList[i]).pMetaData;
      size_t nMetaDataByte = 0;
      uint8_t nMetaDataBit = 0;
      for(; j < pMatrix->b; j++) {
        if((pMetaData[nMetaDataByte] >> nMetaDataBit) & 1) {
          uint32_t word = (pBlock->nVariables+j)/64;
          uint64_t bit = (pBlock->nVariables+j)%64;
          ((uint64_t *)pMatrix->matrix)[i*pMatrix->wds + word] |= (((uint64_t)1) << bit);
        }
        if(++nMetaDataBit == 8) {
          nMetaDataBit = 0;
          nMetaDataByte++;
        }
      }
    }
  }

  return pMatrix;
}

void IMMIRGetSparseRow(uint64_t *tmpRow, gf2_t *pMatrix, uint32_t nRowNum) {
  uint32_t i;
  tmpRow[0] = 0;
  for(i = 0; i < pMatrix->wds; i++) {
    uint64_t the_word = ((uint64_t *)pMatrix->matrix)[nRowNum*pMatrix->wds + i];
    uint64_t iw = 0;
    while(the_word) {
      uint8_t next_char = the_word&0xff;
      while(next_char) {
	uint64_t lb = (uint64_t)immir_low_bit[next_char];
	next_char ^= (1<<lb);
	tmpRow[++tmpRow[0]] = iw+lb + (i << 6);
	assert(tmpRow[0] < pMatrix->n + pMatrix->b);
	assert(tmpRow[tmpRow[0]] < (pMatrix->n + pMatrix->b));
      }
      iw += 8;
      the_word >>= 8;
    }
  }
}

uint8_t XORSATFilterFindIMMIRSolutions(gf2_t *pMatrix, bitvector_t *pSolutions) {
  int32_t i, j;
  uint64_t pRow[pMatrix->n + pMatrix->b];

  int ret = gf2_semi_ech(pMatrix);

  for(i = 0; i < pMatrix->n; i++) {
    for(j = 0; j < pSolutions[i].bits.nLength; j++) {
      pSolutions[i].bits.pList[j] ^= ((uint64_t) rand()) <<  0;
      pSolutions[i].bits.pList[j] ^= ((uint64_t) rand()) << 32;
    }
  }
  
  bitvector_t *pParity = bitvector_t_alloc(pMatrix->b);
  
  for(i = pMatrix->m-1; i >= 0; i--) {
    IMMIRGetSparseRow(pRow, pMatrix, i);

    if(pRow[0] == 0) continue;

    bitvector_t_zeroize(pParity);
    while(pRow[0] && (pRow[pRow[0]] >= pMatrix->n)) {
      bitvector_t_setBit(pParity, pRow[pRow[0]] - pMatrix->n, 1);
      pRow[0]--;
    }

    if(pRow[0] == 0) {
      return 0; //UNSAT
      //Could be duplicate original row that was zeroed out. Wouldn't
      //know unless checking whether the non-metatdata bits of the RHS
      //are all zero. However, this is unlikely as duplicate rows were
      //removed near the top of
      //xorsat_blocks.c:XORSATFilterDistributeHashesToBlocks. This
      //case could still happen if two hashes produce the same row,
      //but then the block will be resized until this doesn't
      //happen. So, the result is that efficiency may suffer, but only
      //slightly. Or, one could put the appropriate check here which
      //would involve passing in the number of solutions.
    }

    for(j = 2; j <= pRow[0]; j++) {
      bitvector_t_xorUpdate(pParity, &pSolutions[pRow[j]]);
    }
    
    assert(pRow[1] < pMatrix->n);
    bitvector_t_copyUpdate(&pSolutions[pRow[1]], pParity);
  }

  bitvector_t_free(pParity);
  
  return 1;  
}
