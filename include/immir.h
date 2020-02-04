#ifndef IMMIR_H
#define IMMIR_H

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <math.h>
#include <assert.h>
#include <inttypes.h>

// ----------------------------------------------------------------------
// data structure for managing information related to solving
// linear systems over GF(2)...

typedef struct {
  // --- required user input parameters:
  int m;              // number of rows
  int n;              // number of (bit) columns
  int b;              // number of rhs columns
  // --- optional paramters (defaults will be computed)
  int wds;            // width of matrix in words
  int kmax;           // max number of kernel vectors (user can set to limit them)
  int tablebits;      // number of bits for 4 Russians tables
  // --- flags
  int inited;         // inited?
  int ech;            // flag to indicate semi-ech form
  // flags to indicate the corresponding arrays need to be free'd
  // (the user may have provided their own arrays, for example)
  int free_matrix, free_kernel, free_solution, free_pivots;
  // --- storage
  void *matrix;       // flattened array of m * wds words
  void *kernel;       // flattened array of k * wds words
  void *solution;     // if non-null, a particular solution  (b * wds words)
  int  *pivots;       // pivots
  // --- computed data
  int rank;           // rank of system
  int corank;         // rank of kernel
} gf2_t;

void gf2_init(gf2_t *data);
void gf2_clear(gf2_t *data);
int gf2_semi_ech(gf2_t *data);
int gf2_solution(gf2_t *data);
int gf2_kernel(gf2_t *data);
  
#endif
