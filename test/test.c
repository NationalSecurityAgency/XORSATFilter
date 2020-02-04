/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "xorsat_filter.h"

int main(int argc, char **argv) {
  uint64_t nElements = 1000000;
  size_t nElementBytes = 10;
  size_t nMetaDataBytes = 10;
  uint32_t nThreads = 16;
  uint64_t i, j;
  
  struct timeval tv1;
  struct timezone tzp1;  
  gettimeofday(&tv1, &tzp1);
  uint32_t random_seed = ((tv1.tv_sec & 0177) * 1000000) + tv1.tv_usec;
  
  fprintf(stderr, "random seed = %d\n", random_seed);
  srand(random_seed);
  
  XORSATFilterBuilder *xsfb = XORSATFilterBuilderAlloc(nElements, nMetaDataBytes);
  if(xsfb == NULL) {
    fprintf(stderr, "malloc() failed...exiting\n");
    return -1;
  }
  
  fprintf(stdout, "\nAdding %"PRIu64" elements to filter\n", nElements);
  
  uint8_t *pElement = malloc(nElementBytes * sizeof(uint8_t));
  if(pElement == NULL) {
    fprintf(stderr, "malloc() failed...exiting\n");
    return -1;
  }
  
  uint8_t *pMetaData = malloc(nMetaDataBytes * sizeof(uint8_t));
  if(pElement == NULL) {
    fprintf(stderr, "malloc() failed...exiting\n");
    return -1;
  }
  
  time_t start_wall = time(NULL);
  clock_t start_cpu = clock();
  
  for(i = 0; i < nElements; i++) {
    for(j = 0; j < nElementBytes; j++) {
      pElement[j] = (uint8_t)(rand()%256);
    }
    for(j = 0; j < nMetaDataBytes; j++) {
      pMetaData[j] = (uint8_t)(rand()%256);
    }
    if(i % 10 == 0) {
      if(XORSATFilterBuilderAddAbsence(xsfb, pElement, nElementBytes) != 0) {
        fprintf(stderr, "Element absence insertion failed...exiting\n");
        return -1;
      }
    } else {
      if(XORSATFilterBuilderAddElement(xsfb, pElement, nElementBytes, pMetaData) != 0) {
        fprintf(stderr, "Element insertion failed...exiting\n");
        return -1;
      }
    }
  }

  fprintf(stdout, "\nBuilding filter\n");

  XORSATFilterQuerier *xsfq =
    XORSATFilterBuilderFinalize(xsfb,
                                //XORSATFilterEfficientParameters,
                                XORSATFilterPaperParameters,
                                //XORSATFilterFastParameters,
                                nThreads);

  clock_t end_cpu = clock();
  time_t end_wall = time(NULL);
  double time_wall = difftime(end_wall, start_wall);
  double time_cpu = ((double) (end_cpu - start_cpu)) / (double) CLOCKS_PER_SEC;

  XORSATFilterBuilderFree(xsfb);

  if(xsfq == NULL) {
    fprintf(stderr, "Finalization failed...exiting\n");
    return -1;
  }

  fprintf(stdout, "Building took %1.0lf wallclock seconds and %1.0lf CPU seconds\n", time_wall, time_cpu);
  
  FILE *fout = fopen("filter.xor", "w");
  if(XORSATFilterSerialize(fout, xsfq) != 0) {
    fprintf(stderr, "Serialization failed...exiting\n");
    return -1;
  }
  fclose(fout);
  XORSATFilterQuerierFree(xsfq);

  fout = fopen("filter.xor", "r");
  xsfq = XORSATFilterDeserialize(fout);
  if(xsfq == NULL) {
    fprintf(stderr, "Deserialization failed...exiting\n");
    return -1;
  }
  fclose(fout);
  
  //Test Querier
  srand(random_seed);

  fprintf(stdout, "\nTesting against original elements\n");
  
  uint64_t nNoes = 0;
  for(i = 0; i < nElements; i++) {
    for(j = 0; j < nElementBytes; j++) {
      pElement[j] = (uint8_t)(rand()%256);
    }
    for(j = 0; j < nMetaDataBytes; j++) {
      pMetaData[j] = (uint8_t)(rand()%256);
    }
    uint8_t ret = XORSATFilterQuery(xsfq, pElement, nElementBytes);

    if(i % 10 == 0) {
      if(ret == 1) {
        nNoes++;
        //fprintf(stderr, "\neek!\n");
      }
    } else {
      if(ret == 0) {
        //fprintf(stderr, "\neek!\n");
        nNoes++;
      } else {
        uint8_t *pMetaData_retrieved = XORSATFilterRetrieveMetadata(xsfq, pElement, nElementBytes);
        if((nMetaDataBytes > 0 && pMetaData_retrieved == NULL) ||
           (strncmp((const char *)pMetaData_retrieved, (const char *)pMetaData, nMetaDataBytes) != 0)) {
          fprintf(stderr, "Metadata retrieval failed.\n");
        }
        free(pMetaData_retrieved);
      }
    }
  }

  double p = 1.0 - (i==0 ? 0.0 : ((double)nNoes)/((double)i));
  fprintf(stdout, "Percent passed = %4.4lf%%\n", p*100.0);
  assert(p == 1.0);
  free(pElement);
  free(pMetaData);

  fprintf(stdout, "\nTesting query speed with util func: %u queries per second\n", XORSATFilterQueryRate(xsfq));

  if(nMetaDataBytes > 0) {
    fprintf(stdout, "\nTesting metadata retrieval speed with util func: %u retrievals per second\n", XORSATFilterMetadataRetrievalRate(xsfq, nElementBytes));
  }

  p = XORSATFilterFalsePositiveRate(xsfq);
  fprintf(stdout, "Testing false positive rate with util func: %4.8lf%%\n", p * 100.0);

  fprintf(stdout, "Testing filter efficiency with util func: %4.2lf%% efficient\n", XORSATFilterEfficiency(xsfq, nElements, p) * 100.0);
  fprintf(stdout, "Filter uses %4.2lf bits per element\n", ((double) XORSATFilterSize(xsfq)) / (double) nElements);

  fprintf(stdout, "Testing metadata efficiency with util func: %4.2lf%% efficient\n", XORSATMetaDataEfficiency(xsfq, nElements) * 100.0);
  fprintf(stdout, "Metadata uses %4.2lf bits per element\n", ((double) XORSATMetaDataSize(xsfq)) / (double) nElements);

  fprintf(stdout, "Seralized object uses %"PRIu64" bits\n", XORSATAncillarySize(xsfq) + XORSATMetaDataSize(xsfq) + XORSATFilterSize(xsfq));
  
  XORSATFilterQuerierFree(xsfq);
  
  return 0;
}
