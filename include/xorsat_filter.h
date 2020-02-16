/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATFILTER_H
#define XORSATFILTER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

#include "../lib/c_list_types/include/c_list_types.h"
#include "../lib/bitvector/include/bitvector.h"
#include "../lib/C-Thread-Pool/thpool.h"

#include "list_types.h"
#include "xorsat_hashes.h"
#include "xorsat_metadata.h"
#include "xorsat_blocks.h"
#include "xorsat_solve.h"
#include "xorsat_immir_wrap.h"

//Example paramters can be found in src/xorsat_blocks.c
typedef struct XORSATFilterParameters {
  uint8_t nLitsPerRow;    //Suggest between 3 and 7
                          //  Small numbers provide faster querying
                          //  Large numbers provide more easily achievable higher efficiency
  uint8_t nSolutions;     //False positive rate of the filter will be approx. 2^-nSolutions
                          //nSolutions must be less than or equal to 32 (for now)
  uint16_t nEltsPerBlock; //Must be less than 65536 due to how variables are created from hashes
                          //  Large numbers provide higher efficiency
                          //  Small numbers provide faster build time
  double fEfficiency;     //Desired efficiency, between 0.0 and 1.0
                          //  For best results, set this number to just above the actual achieved efficiency
                          //  This can be determined by testing
} XORSATFilterParameters;

extern XORSATFilterParameters XORSATFilterEfficientParameters;
extern XORSATFilterParameters XORSATFilterPaperParameters;
extern XORSATFilterParameters XORSATFilterFastParameters;

//Comment out the following to silence progress updates in `XORSATFilterBuilderFinalize`
#define XORSATFILTER_PRINT_BUILD_PROGRESS

typedef struct XORSATFilterBuilder {
  XORSATFilterHash_list pHashes;
  size_t nMetaDataBytes;
  XORSATFilterMetaData_list pMetaData;
  XORSATFilterBlock_list pBlocks;
} XORSATFilterBuilder;


typedef struct XORSATFilterQuerier {
  uint64_t *pFilter;
  int16_t *pOffsets;
  uint32_t nBlocks;
  uint8_t nSolutions;
  size_t nMetaDataBytes;
  uint16_t nAvgVarsPerBlock;
  uint8_t nLitsPerRow;
  uint8_t  bMMAP;
} XORSATFilterQuerier;

#include "xorsat_serial.h"

XORSATFilterBuilder *XORSATFilterBuilderAlloc(uint32_t nExpectedElements, size_t nMetaDataBytes);
void XORSATFilterBuilderFree(XORSATFilterBuilder *xsfb);
uint8_t XORSATFilterBuilderAddElement(XORSATFilterBuilder *xsfb, const void *pElement, size_t nElementBytes, const void *pMetaData);
uint8_t XORSATFilterBuilderAddAbsence(XORSATFilterBuilder *xsfb, const void *pElement, size_t nElementBytes);
XORSATFilterQuerier *XORSATFilterBuilderFinalize(XORSATFilterBuilder *xsfb, XORSATFilterParameters sParams, uint32_t nThreads);

void XORSATFilterQuerierFree(XORSATFilterQuerier *xsfq);

uint8_t XORSATFilterQuery(XORSATFilterQuerier *xsfq, const void *pElement, uint32_t nElementBytes);
uint8_t *XORSATFilterRetrieveMetadata(XORSATFilterQuerier *xsfq, const void *pElement, uint32_t nElementBytes);

uint32_t XORSATFilterQueryRate(XORSATFilterQuerier *xsfq);
uint32_t XORSATFilterMetadataRetrievalRate(XORSATFilterQuerier *xsfq, uint32_t nElementBytes);
double XORSATFilterFalsePositiveRate(XORSATFilterQuerier *xsfq);
uint64_t XORSATAncillarySize(XORSATFilterQuerier *xsfq);
uint64_t XORSATFilterSize(XORSATFilterQuerier *xsfq);
uint64_t XORSATMetaDataSize(XORSATFilterQuerier *xsfq);
double XORSATFilterEfficiency(XORSATFilterQuerier *xsfq, uint64_t nElements, double p);
double XORSATMetaDataEfficiency(XORSATFilterQuerier *xsfq, uint64_t nElements);

uint64_t XORSATFilterGetBlockIndex(XORSATFilterQuerier *xsfq, uint32_t nBlock);

#endif
