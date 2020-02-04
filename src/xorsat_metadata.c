/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#include "xorsat_filter.h"

create_c_list_type(XORSATFilterMetaData_list, XORSATFilterMetaData)

void XORSATFilterMetaDataFree(XORSATFilterMetaData *pMetaData) {
  if(pMetaData != NULL) {
    if(pMetaData->pMetaData != NULL) {
      free(pMetaData->pMetaData);
    }
  }
}
