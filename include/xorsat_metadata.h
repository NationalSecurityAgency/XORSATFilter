/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATMETADATA_H
#define XORSATMETADATA_H

typedef struct XORSATFilterMetaData {
  uint8_t *pMetaData;
} XORSATFilterMetaData;

create_c_list_headers(XORSATFilterMetaData_list, XORSATFilterMetaData)

void XORSATFilterMetaDataFree(XORSATFilterMetaData *pData);
  
#endif
