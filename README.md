INTRODUCTION
============

This is a library for building and querying a compressed form of
set-membership filters, named k-XORSAT filters. These filters can be
used similar to how one would use a Bloom filter but with one
restriction --- items cannot be added after the filter is built. So,
this is an 'offline' or 'static' filter, whereas Bloom filters are
considered 'online' or 'dynamic'. The advantage is that k-XORSAT
filters achieve very near the optimal memory usage. That is, they use
much less memory than Bloom filters, making them desirable for either
large data sets (save space) or data sets that are provided to a large
user base (save bandwidth).

The advantage of k-XORSAT filters over other filters with small memory
footprint is that the near-optimal compression of k-XORSAT filters is
reliably achieved, and the query speed is comparable to Bloom filters
for high false positive rates, and faster than Bloom filters for small
false positive rates.


DEPENDENCIES
============

This package depends on standard C math libraries.

This project also relies on a few git submodules. To get these
modules, clone the repository by doing either
```
git clone --recursive git@github.com:NationalSecurityAgency/XORSATFilter.git
```
or
```
git clone git@github.com:NationalSecurityAgency/XORSATFilter.git
cd XORSATFilter
git submodule update --init --recursive
```


INSTALL
=======

Run `make` in the project root directory. The library file
`libxorsatfilter.a` will (assuming successful compilation) be created
in this package's `lib` directory.


USE
===

k-XORSAT filters are built in two separate phases. The first step is
to add elements to a builder object. The second phase is to create a
querier object from a builder object. Once completed, the querier
object may be queried ad infinitum.

A builder is first allocated using `XORSATFilterBuilderAlloc`,
like so:

```
  XORSATFilterBuilder *xsfb = XORSATFilterBuilderAlloc(0, 0);
```

Here, the first argument `0` is the number of expected elements the
filter will encode. It is safe to leave this number as `0`, but will
decrease calls to malloc if the number is given ahead of time. The
second argument is the number of bytes of metatdata to store with each
element. To use the data structure as a filter, enter 0. To use the
data structure as a dictionary, enter the number of bytes to store in
the dictionary.

Elements are added to the builder, like so:

```
  XORSATFilterBuilderAddElement(xsfb, pElement, nElementBytes, pMetaData);
```

Here, `pElement` is a pointer to at least `nElementBytes` number of
bytes. This element will be copied into the builder. `pMetaData` is a
pointer to an array of bytes that will be stored into the data
structure and can be retrieved after construction. If the data
structure is used as a filter, this second argument may be `NULL`.

After all elements have been stored, the querier is ready to be
created:

```
  XORSATFilterQuerier *xsfq = XORSATFilterBuilderFinalize(xsfb, XORSATFilterFastParameters, nThreads);
```

The second argument is a structure consisting of four parameters: the
number of literals per row, the number of solutions, the number of
elements per block, and the desired efficiency of the filter. Three
samples are provided by the package,

  1) `XORSATFilterEfficientParameters` creates small filters but has longer
build time.

  2) `XORSATFilterPaperParameters` creates filters according to parameters used in a recent paper on XORSAT filters.

  3) `XORSATFilterFastParameters` creates filters quickly but
the filters are larger.

More information can be found near the tops of
`include/xorsat_filter.h` and `src/xorsat_blocks.c`. Feel free to
define your own parameters to meet the needs of your application.

The third `nThreads` argument corresponds to the number of pthreads
used when building the querier. The returned querier (`xsfq`) will be
`NULL` on error.

When finalizing, you will notice that the progress is printed to
stderr. These print statements can be turned off by commenting out the
following line in xorsat_filter.h and recompiling the package.

```
#define XORSATFILTER_PRINT_BUILD_PROGRESS
```

After creating the querier, it is suggested that the builder be
free'd, like so:

```
  XORSATFilterBuilderFree(xsfb);
```

The filter can be queried, like so:

```
  uint8_t ret = XORSATFilterQuery(xsfq, pElement, nElementBytes);
```

Here, `pElement` is a pointer to `nElementBytes` number of bytes. If
this element might be in the filter (depending on the false positive
rate), `ret` will return `1`. Otherwise, `ret` will return `0`,
indicating that this element is definitely not in the filter.

Stored metadata can be retrived like so:

```
  uint8_t *pMetaData_retrieved = XORSATFilterRetrieveMetadata(xsfq, pElement, nElementBytes);
```

If `pElement` was not added to the data structure, the metadata
returned will appear random. Otherwise, the stored metadata will be
returned via a newly allocated pointer.

Queriers can be serialized (written to a file) in the following way:

```
  uint8_t ret = XORSATFilterSerialize(fout, xsfq);
```

Here, `fout` is of type `FILE *`. `ret` will be `0` on failure and `1`
on success.

A querier can be deserialized (read from a file) in the following way:

```
  xsfq = XORSATFilterDeserialize(fout);
```

Here, `fout` is of type `FILE *`. `xsfq` will be `NULL` on error.

When querying is done, the filter can be freed, like so:

```
  XORSATFilterQuerierFree(xsfq);
```

To use, simply link against `lib/libxorsatfilter.a` and include
`include/xorsat_filter.h`.

This interface is demonstrated in a test provided with this library.


TEST
====

A sample interface is given in the `test` directory. The test builds a
k-XORSAT dictionary for 1000000 random 10-byte elements each with
10-bytes of metadata, writes the dictionary to a file, reads the file
back, then queries the filter against the original elements (for a
consistency check) and prints statistics. To run the test type:

```
$ make test/test && test/test
```


FURTHER INFORMATION
==================

A paper about SAT Filters is published in JSAT, available here:
[Satisfiability-based Set Membership
Filters](http://satassociation.org/jsat/index.php/jsat/article/view/102)

A paper about XORSAT Filters is published in SAT'18, available here:
[XOR-Satisfiability Set Membership
Filters](https://link.springer.com/chapter/10.1007/978-3-319-94144-8_24)


LICENSE
=======

This work was prepared by U.S. Government employees and, therefore, is
excluded from copyright by Section 105 of the Copyright Act of 1976.

Copyright and Related Rights in the Work worldwide are waived through
the [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
[Universal license](https://creativecommons.org/publicdomain/zero/1.0/legalcode).


DISCLAIMER
==========

This Work is provided "as is." Any express or implied warranties,
including but not limited to, the implied warranties of
merchantability and fitness for a particular purpose are
disclaimed. In no event shall the United States Government be liable
for any direct, indirect, incidental, special, exemplary or
consequential damages (including, but not limited to, procurement of
substitute goods or services, loss of use, data or profits, or
business interruption) however caused and on any theory of liability,
whether in contract, strict liability, or tort (including negligence
or otherwise) arising in any way out of the use of this Guidance, even
if advised of the possibility of such damage.

The User of this Work agrees to hold harmless and indemnify the United
States Government, its agents and employees from every claim or
liability (whether in tort or in contract), including attorneys' fees,
court costs, and expenses, arising in direct consequence of
Recipient's use of the item, including, but not limited to, claims or
liabilities made for injury to or death of personnel of User or third
parties, damage to or destruction of property of User or third
parties, and infringement or other violations of intellectual property
or technical data rights.

Nothing in this Work is intended to constitute an endorsement,
explicit or implied, by the U.S. Government of any particular
manufacturer's product or service.
