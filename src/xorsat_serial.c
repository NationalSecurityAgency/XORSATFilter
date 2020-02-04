/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

uint8_t XORSATFilterSerialize(FILE *pXORSATFilterFile, XORSATFilterQuerier *xsfq) {
  if(pXORSATFilterFile == NULL) return 1; //Failure

  size_t write;

  uint64_t nFilterWords = XORSATFilterGetBlockIndex(xsfq, xsfq->nBlocks);

  //Write filter
  write = fwrite(xsfq->pFilter, sizeof(uint64_t), nFilterWords, pXORSATFilterFile);
  if(write != nFilterWords) return 1; //Failure

  //Write block offsets from expected
  write = fwrite(xsfq->pOffsets, sizeof(int16_t), xsfq->nBlocks+1, pXORSATFilterFile);
  if(write != (xsfq->nBlocks+1)) return 1; //Failure

  //Write filter header
  XORSATFilterSerialData xsfsd = {.nBlocks = xsfq->nBlocks,
				  .nAvgVarsPerBlock = xsfq->nAvgVarsPerBlock,
				  .nSolutions = xsfq->nSolutions,
				  .nMetaDataBytes = xsfq->nMetaDataBytes,
				  .nLitsPerRow = xsfq->nLitsPerRow};
  write = fwrite(&xsfsd, sizeof(XORSATFilterSerialData), 1, pXORSATFilterFile);
  if(write != 1) return 1; //Failure

  return 0; //Success
}

XORSATFilterQuerier *XORSATFilterDeserialize(FILE *pXORSATFilterFile) {
  if(pXORSATFilterFile == NULL) return NULL;

  int seek = fseek(pXORSATFilterFile, -(sizeof(uint16_t) + sizeof(XORSATFilterSerialData)), SEEK_END);
  if(seek != 0) return NULL;

  //Record the file size (minus the header data) for later verification
  uint64_t nDataSize = ftell(pXORSATFilterFile) + sizeof(uint16_t);
  
  size_t read;

  //Read the last block offset (needed to compute the size of the filter)
  int16_t nDiff;
  read = fread(&nDiff, sizeof(int16_t), 1, pXORSATFilterFile);
  if(read != 1) return NULL;  

  //Read filter header
  XORSATFilterSerialData xsfsd;
  read = fread(&xsfsd, sizeof(XORSATFilterSerialData), 1, pXORSATFilterFile);
  if(read != 1) return NULL;

  if(xsfsd.nSolutions > 32) {
    fprintf(stderr, "Error: nSolutions must be <= 32\n"); //For now
    return NULL;
  }

  /*
  if(xsfsd.nSolutions + (xsfsd.nMetaDataBytes * 8) > 64) {
    fprintf(stderr, "Error: nSolutions + (nMetaDataBytes * 8) cannot be greater than 64\n"); //For now
    return NULL;
  }
  */
  
  XORSATFilterQuerier *xsfq = (XORSATFilterQuerier *)malloc(1 * sizeof(XORSATFilterQuerier));
  if(xsfq == NULL) return NULL;
  xsfq->nBlocks = xsfsd.nBlocks;
  xsfq->nAvgVarsPerBlock = xsfsd.nAvgVarsPerBlock;
  xsfq->nSolutions = xsfsd.nSolutions;
  xsfq->nMetaDataBytes = xsfsd.nMetaDataBytes;
  xsfq->nLitsPerRow = xsfsd.nLitsPerRow;

  //Compute the size of the filter
  //See xorsat_query::XORSATFilterGetBlockIndex for more info
  int64_t nExpectedIndex = ((int64_t) xsfq->nAvgVarsPerBlock) * (int64_t) xsfq->nBlocks;
  //Round up to next multiple of 64
  nExpectedIndex = ((nExpectedIndex-1) | (int64_t) 0x3f) + (int64_t) 1;
  uint64_t nFilterWords = ((nExpectedIndex - ((int64_t) nDiff * 64)) >> 6) * (uint64_t) (xsfq->nSolutions + (xsfq->nMetaDataBytes*8));

  if(nDataSize != (nFilterWords * sizeof(uint64_t)) + ((xsfq->nBlocks+1) * sizeof(int16_t))) {
    fprintf(stderr, "Error: filter file is corrupt\n");
    free(xsfq);
    return NULL;
  }
  
  //Read filter and block offsets from expected
  xsfq->pFilter = (uint64_t *)mmap(0, (nFilterWords * sizeof(uint64_t)) + ((xsfq->nBlocks+1) * sizeof(int16_t)), PROT_READ, MAP_PRIVATE, fileno(pXORSATFilterFile), 0);

  xsfq->pOffsets = (int16_t *) (xsfq->pFilter + nFilterWords);

  xsfq->bMMAP = 1;

  return xsfq;
}
