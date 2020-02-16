/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

/* nExpectedElements can be 0 */
XORSATFilterBuilder *XORSATFilterBuilderAlloc(uint32_t nExpectedElements, size_t nMetaDataBytes) {
  XORSATFilterBuilder *xsfb = (XORSATFilterBuilder *)malloc(1 * sizeof(XORSATFilterBuilder));
  if(xsfb == NULL) return NULL;

  if(XORSATFilterHash_list_init(&xsfb->pHashes, nExpectedElements) != C_LIST_NO_ERROR) {
    free(xsfb);
    return NULL;
  }

  xsfb->nMetaDataBytes = nMetaDataBytes;
  
  if(xsfb->nMetaDataBytes > 0) {
    if(XORSATFilterMetaData_list_init(&xsfb->pMetaData, nExpectedElements) != C_LIST_NO_ERROR) {
      free(xsfb);
      return NULL;
    }
  }
  
  if(XORSATFilterBlock_list_init(&xsfb->pBlocks, 0) != C_LIST_NO_ERROR) {
    free(xsfb);
    return NULL;
  }
  
  return xsfb;
}

void XORSATFilterBuilderFree(XORSATFilterBuilder *xsfb) {
  XORSATFilterHash_list_free(&xsfb->pHashes, NULL);
  if(xsfb->nMetaDataBytes > 0) {
    XORSATFilterMetaData_list_free(&xsfb->pMetaData, XORSATFilterMetaDataFree);
  }
  XORSATFilterBlock_list_free(&xsfb->pBlocks, XORSATFilterBlockFree);
  free(xsfb);
}

uint8_t XORSATFilterBuilderAddElement(XORSATFilterBuilder *xsfb, const void *pElement, size_t nElementBytes, const void *pMetaData) {
  XORSATFilterHash pHash = XORSATFilterGenerateHashesFromElement(pElement, nElementBytes);

  if(xsfb->nMetaDataBytes > 0) {
    if(pMetaData == NULL) {
      fprintf(stderr, "Metadata expected, but none found...\n");
      return 1;
    }
    XORSATFilterMetaData MetaDataCopy;
    MetaDataCopy.pMetaData = (uint8_t *)malloc(xsfb->nMetaDataBytes * sizeof(uint8_t));
    memcpy((void *)MetaDataCopy.pMetaData, (void *)pMetaData, xsfb->nMetaDataBytes * sizeof(uint8_t));
    if(MetaDataCopy.pMetaData == NULL) {
      fprintf(stderr, "malloc() failed when copying metadata\n");
      return 1;
    }
    uint8_t ret = XORSATFilterMetaData_list_push(&xsfb->pMetaData, MetaDataCopy);
    if(ret != C_LIST_NO_ERROR) return ret;
  }

  return XORSATFilterHash_list_push(&xsfb->pHashes, pHash);
}

uint8_t XORSATFilterBuilderAddAbsence(XORSATFilterBuilder *xsfb, const void *pElement, size_t nElementBytes) {
  XORSATFilterHash pHash = XORSATFilterGenerateHashesFromElement(pElement, nElementBytes);

  if(xsfb->nMetaDataBytes > 0) {
    XORSATFilterMetaData MetaDataCopy;
    MetaDataCopy.pMetaData = (uint8_t *)malloc(xsfb->nMetaDataBytes * sizeof(uint8_t));
    if(MetaDataCopy.pMetaData == NULL) {
      fprintf(stderr, "malloc() failed when copying metadata\n");
      return 1;
    }
    uint8_t ret = XORSATFilterMetaData_list_push(&xsfb->pMetaData, MetaDataCopy);
    if(ret != C_LIST_NO_ERROR) return ret;
  }

  pHash.present = 0;
  
  return XORSATFilterHash_list_push(&xsfb->pHashes, pHash);
}

uint8_t XORSATFilterDistributeHashesToBlocks(XORSATFilterBuilder *xsfb, XORSATFilterParameters sParams);
XORSATFilterQuerier *XORSATFilterCreateQuerierFromBuilder(XORSATFilterBuilder *xsfb);

XORSATFilterQuerier *XORSATFilterBuilderFinalize(XORSATFilterBuilder *xsfb, XORSATFilterParameters sParams, uint32_t nThreads) {
  uint8_t ret;
  uint32_t i;
  XORSATFilterQuerier *xsfq = NULL;

  //Sanity check parameters
  if(sParams.nSolutions > 32) {
    fprintf(stderr, "Error: XORSATFilterParameters.nSolutions must be <= 32\n"); //For now
    return NULL;
  }

  if(sParams.nLitsPerRow > 20) {
    //20 is a bit arbitrary.
    fprintf(stderr, "Error: XORSATFilterParameters.nLitsPerRow must be <= 20\n");
    return NULL;
  }

  /*
  if(sParams.nSolutions + (xsfb->nMetaDataBytes*8) > 64) {
    fprintf(stderr, "Error: XORSATFilterParameters.nSolutions + (nMetaDataBytes*8) cannot be greater than 64\n");
    return NULL;
  }
  */
  
  if(xsfb->nMetaDataBytes > 0 && (xsfb->pHashes.nLength != xsfb->pMetaData.nLength)) {
    fprintf(stderr, "Meta Data storage corrupted\n");
    return NULL;
  }
  
  if(sParams.nEltsPerBlock > xsfb->pHashes.nLength) {
    sParams.nEltsPerBlock = xsfb->pHashes.nLength;
  }

  if(sParams.fEfficiency > 1.0) {
    fprintf(stderr, "Warning: XORSATFilterParameters.fEfficiency must be <= 1. Setting to 1\n");
    sParams.fEfficiency = 1.0;
  }

  ret = XORSATFilterDistributeHashesToBlocks(xsfb, sParams);
  if(ret != 0) return NULL;


  //Build Blocks
  threadpool thpool = thpool_init(nThreads);
  for(i = 0; i < xsfb->pBlocks.nLength; i++) {
    XORSATFilterBlock *pBlock = &xsfb->pBlocks.pList[i];
    thpool_add_work(thpool, (void*)XORSATFilterSolveBlock, pBlock);
  }

  thpool_wait(thpool);
  thpool_destroy(thpool);
  
  xsfq = XORSATFilterCreateQuerierFromBuilder(xsfb);

  return xsfq;
}
