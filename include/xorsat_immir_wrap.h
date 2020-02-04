/**************************************************************************************

  XORSAT Filter: A library for building and querying k-XORSAT set-membership filters.

**************************************************************************************/

#ifndef XORSATIMMIRWRAP_H
#define XORSATIMMIRWRAP_H

#include "immir.h"

gf2_t *XORSATFilterBuildIMMIRMatrix_DW(XORSATFilterBlock *pBlock);
gf2_t *XORSATFilterBuildIMMIRMatrix_WRS(XORSATFilterBlock *pBlock);
uint8_t XORSATFilterFindIMMIRSolutions(gf2_t *pMatrix, bitvector_t *pSolutions);

#endif
