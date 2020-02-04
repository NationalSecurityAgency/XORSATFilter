/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATSERIAL_H
#define XORSATSERIAL_H

typedef struct XORSATFilterSerialData {
  uint32_t nBlocks;
  uint16_t nAvgVarsPerBlock;
  uint8_t nSolutions;
  size_t nMetaDataBytes;
  uint8_t nLitsPerRow;  
} XORSATFilterSerialData;

uint8_t XORSATFilterSerialize(FILE *pXORSATFilterFile, XORSATFilterQuerier *xsfq);
XORSATFilterQuerier *XORSATFilterDeserialize(FILE *pXORSATFilterFile);

#endif
