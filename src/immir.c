#include "immir.h"

// ----------------------------------------------------------------------
// --- static helper functions should be inlined

static uint64_t bits (uint64_t *V, int i, int b) {
  int w0 = i/64, w1 = (i+b-1)/64, ii = i%64;
  uint64_t x = V[w0] >> ii;
  if (w0 < w1)
    x ^= V[w1] << (64 - ii);
  x &= (1UL<<b)-1;
  return x;
}

static uint64_t bit (uint64_t *V, int i) {
  uint64_t probe = 1UL << (i%64);
  return (V[i/64] & probe) != 0;
}

static void addrows (uint64_t * restrict A, uint64_t * restrict B, uint64_t * restrict C, int s, int wds) {
  for (int w = s; w < wds; w++)
    A[w] = B[w] ^ C[w];
}

static uint64_t dotprod (uint64_t *A, uint64_t *B, int w0, int w1) {
  uint64_t x = 0;
  for (int w = w0; w < w1; w++)
    x ^= A[w] & B[w];
  return __builtin_parityl(x);
}

// ----------------------------------------------------------------------
// main worker functions

static int semi_ech(const int m, const int n, const int wds, uint64_t (*A)[wds],
                    int S, int *piv) {

  // 4 Russians with S bit tables; Z[] holds the precomputed row sums,
  // z[] the indexing into Z[] required since we don't reduce above the
  // block diagonal...

  if (S == 0) S = log(m); // reasonable default

  const int SS = (1<<S);
  // could skip mallocs here if S < 2
  uint64_t (*Z)[wds] = malloc(SS * sizeof *Z); // Z[SS][wds]
  int *z = malloc(SS * sizeof *z);
  assert(Z);
  assert(z);

  int r = 0; // row in reduction
  int s = 0; // block start for 4 Russians table

  if (piv) for (int r=0; r<m; r++) piv[r] = -1;

  /* 
     This diagram summarises the method. We use "4 Russian" tables
     of width S bits (S=3 below); current block starts at column s;
     to find pivot for row r, we start at j=r and reduce to the
     left using pivots s .. r-1; then check whether row j has a
     pivot for row r -- if it does, (possibly) xor it onto row r.
     Once the block is complete, form the table of size 2^S and
     reduce below...

         +-----+--------------------------------+
         |1 * *|* * * * * * * * * * * * * * * * |
         |  1 *|* * * * * * * * * * * * * * * * |
         |    1|* * * * * * * * * * * * * * * * |
         +-----+-----+--------------------------+
     s-> |     |1 * *|* * * * * * * * * * * * * |
         |     |0 0 *|* * * * * * * * * * * * * | <- r
         |     |0 0 *|* * * * * * * * * * * * * |
         |     |* * *|* * * * * * * * * * * * * | <- j
         |     |* * *|* * * * * * * * * * * * * |
         |     |* * *|* * * * * * * * * * * * * |
         +--------------------------------------+

      On failure to find a pivot, break out for a simpler loop
      that finishes it off.

   */

  for (; S > 1 && s + S <= m && r == s; s += S) {

    // S columns at a time for 4 Russians method

    // NB: when we break out of following loop because there
    // is no pivot for column r, then the condition r==s above
    // will be violated.

    for (r = s; r < s + S; r++) {

      // find a row with pivot in column r
      int j;
      for (j = r; j < m; j++) {
        for (int k = s; k < r; k++)
          // reduce relative to this block using pivots found so far
          if (bit(A[j], k))
            addrows (A[j], A[j], A[k], s/64, wds);
        // now check for new pivot
        if (bit(A[j], r)) break;
      }

      if  (j == m)
        // no pivot in this column, skip to next section
        break;

      if (j != r) 
        // xor onto row r to get pivot there (if it's not already)
        addrows (A[r], A[r], A[j], s/64, wds);

      if (piv) piv[r] = r;
    }
  
    // missing pivot; break out to clean up loop
    if (r != s + S) break;

    // we have a full block of S new pivots, let's reduce below
    // this block -- unless there are no rows below us...
    if (s + S == m) break;

    // instead of reducing above the block to compute the Z table,
    // we'll figure it out using an array of indices...

    // first, clear Z[0]
    z[0] = 0;
    for (int w = s/64; w < wds; w++)
      Z[0][w] = 0;
    
    // now, for each pivot 0,...,S-1
    for (int i = 0; i < S; i++) {
      int ii = 1<<i;
      int vv = bits(A[s+i], s, S);
      // copy block of size 2^i and xor i-th row onto it
      for (int j = 0; j < ii; j++) {
        int a = z[j], b = a ^ vv;
        z[j+ii] = b;
        addrows (Z[b], Z[a], A[s+i], s/64, wds);
      }
    }

    // now reduce below this full-rank block
    for (int i = s + S; i < m; i++) {
      int c = bits(A[i], s, S);
      addrows (A[i], A[i], Z[c], s/64, wds);
    }

  }

  // at this point, we have rows down to r in upper-triangular
  // form and have cleared below them. We are either missing a
  // pivot, or didn't have enough columns for the 4 Russians method.

  int c = r; // column to probe for pivot
  for (; r < m && c < n;) {
    int j;
    for (j = r; j < m; j++)
      if (bit(A[j], c))
        break;
    if (j == m) { c++; continue; }
    if (j > r)
      for (int w = c/64; w < wds; w++)
        A[r][w] ^= A[j][w];
    assert(bit(A[r],c));
    if (piv) piv[r] = c;
    for (j = r+1; j < m; j++)
      if (bit(A[j], c))
        for (int w = c/64; w < wds; w++)
          A[j][w] ^= A[r][w];
    r++, c++;
  }
  
  free(Z);
  free(z);
  return r;
}

static int kernel(const int m,    // height (rows)
		  const int n,    // width in bits
		  const int wds,  // width in words
		  uint64_t (*A)[wds],
		  const int *piv, // pivots computed during semi-ech
		  const int k,    // number of kernel rows requested
		  uint64_t (*K)[wds]) {

  // returns number of kernel vectors set
  // assumes already in semi-echelon form with pivots in piv[]

  int i = 0;
  for (int r = 0, j = 0; j < n && i < k; i++, j++) {

    // scan forward for next missing pivot:
    while (r < m && j == piv[r])
      j++, r++;
    if (j == n) break;
    
    for (int w=0; w<wds; w++) K[i][w] = 0;
    K[i][j/64] ^= 1UL<<(j%64);
    
    // backsolve
    for (int l=r-1; l>=0; l--) {
      int p = piv[l];
      if (dotprod(A[l], K[i], p/64, j/64+1))
        K[i][p/64] ^= 1UL<<(p%64);
    }
  }

  return i; // number of kernel vectors returned
}

static int solution(const int m,    // height (rows)
             const int n,    // width in bits
             const int wds,  // width in words
             uint64_t (*A)[wds],
             const int *piv, // pivots computed during semi-ech
             const int b,    // number of equation columns
             uint64_t (*X)[wds]) {

  // returns number of solutions; -1 for failure
  // assumes already in semi-echelon form with pivots in piv[]
  // rhs bits are assumed to be bits n,n+1,...,n+b-1

  assert((n+b+63)/64 <= wds);

  int r; // compute rank
  for (r = 0; r < m; r++)
    if (piv[r] < 0) break;

  // check for failure of system
  for (int i = r; i < m; i++)
    if (bits(A[i],n,b) != 0)
      return -1; // failure

  for (int i = 0, j = n; i < b; i++, j++) {

    for (int w=0; w<wds; w++) X[i][w] = 0;

    X[i][j/64] ^= 1UL<<(j%64); // set rhs bit
    
    // backsolve
    for (int l=r-1; l>=0; l--) {
      int p = piv[l];
      assert(p >= 0);
      assert(p < n);
      if (dotprod(A[l], X[i], 0, wds))
        X[i][p/64] ^= 1UL<<(p%64);

    }

    X[i][j/64] ^= 1UL<<(j%64); // clear rhs bit
  }

  return b;
}

void gf2_init(gf2_t *data) {
  //assert(data->n > 0); //Commented out to successfully build filters /w no elements.
  //assert(data->m > 0);
  // user may set data->wds to save space for a RHS or book-keeping columns
  if (data->wds == 0) 
    data->wds = (data->n + data->b + 64-1)/64;
  if (data->tablebits == 0)
    data->tablebits = log(data->m);
  // user may allocate their own array space
  if (data->matrix == NULL) {
    data->matrix = calloc(data->m * data->wds, sizeof(uint64_t));
    assert(data->matrix);
    data->free_matrix = 1;
  }
  // user may allocate their own pivot data
  if (data->pivots == NULL) {
    data->pivots = malloc(data->m * sizeof *data->pivots);
    assert(data->pivots);
    data->free_pivots = 1;
  }
  data->rank = -1;
  data->ech = 0;
  data->inited = 1;
}

void gf2_clear(gf2_t *data) {
  if (data->pivots && data->free_pivots)
    { free(data->pivots); data->pivots = NULL; }
  if (data->matrix && data->free_matrix)
    { free(data->matrix); data->matrix = NULL; }
  if (data->kernel && data->free_kernel)
    { free(data->kernel); data->kernel = NULL; }
  if (data->solution && data->free_solution)
    { free(data->solution); data->solution = NULL; }
  data->inited = 0;
}

int gf2_semi_ech(gf2_t *data) {
  // while (void *) might be evil, the user is responsible for ensuring A points
  // to an array of the appropriate form
  data->rank   = semi_ech(data->m, data->n, data->wds, data->matrix, 
                          data->tablebits, data->pivots);
  data->corank = data->n - data->rank;
  data->ech = 1;
  return data->rank;
}

int gf2_kernel(gf2_t *data) {
  if (!data->ech)
    gf2_semi_ech(data);
  if (data->corank == 0) return 0;
  if (data->kmax == 0)
    data->kmax = data->corank;
  if (data->kernel == NULL) {
    data->kernel = calloc(data->kmax * data->wds, sizeof(uint64_t));
    assert(data->kernel);
    data->free_kernel = 1;
  }
  return kernel(data->m, data->n, data->wds, data->matrix, data->pivots,
                data->kmax, data->kernel);
}

int gf2_solution(gf2_t *data) {
  if (data->b == 0)
    return -1;
  if (!data->ech)
    gf2_semi_ech(data);
  if (data->solution == NULL) {
    data->solution = calloc(data->b * data->wds, sizeof(uint64_t));
    assert(data->solution);
    data->free_solution = 1;
  }
  return solution(data->m, data->n, data->wds, data->matrix, data->pivots,
                  data->b, data->solution);
}

void gf2_info(gf2_t *data) {
  printf("gf2{ m=%d, n=%d, wds=%d, rank=%d, corank=%d }\n",
         data->m, data->n, data->wds, data->rank, data->corank);
}


void dump(const int m, const int n, const int wds,
          uint64_t A[m][wds]) {
  for (int r = 0; r < m; r++) {
    printf(" [%03d] ", r);
    for (int i = 0; i < n; i++)
      printf("%" PRIu64, bit(A[r],i));
    printf("\n");
  }
}
