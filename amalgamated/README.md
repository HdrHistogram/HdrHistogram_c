# Amalgamated HdrHistogram_c core

**Generated files — do not edit by hand.** Regenerate with:

```sh
script/amalgamate.py --output amalgamated
```

This directory is a self-contained, drop-in build of the HdrHistogram_c **core**
(the record path, the value/percentile read path and the iterators). It has no
zlib, threads, logging-codec, interval-recorder or phaser dependency — just the C
standard library and libm.

| file | description |
|------|-------------|
| `hdr_histogram.c` | the engine, with the private headers inlined |
| `hdr_histogram.h` | the public API (unchanged copy) |
| `hdr_malloc.h`    | the default libc allocator hook (unchanged copy) |

## Using it

Copy the three files into your project and build `hdr_histogram.c` with your own
sources:

```sh
cc -c -Ipath/to/amalgamated path/to/amalgamated/hdr_histogram.c
```

To route allocations through your own allocator, drop the default `hdr_malloc.h`
and compile with a header that defines `hdr_malloc` / `hdr_calloc` /
`hdr_realloc` / `hdr_free`:

```sh
cc -c -DHDR_MALLOC_INCLUDE='"my_alloc.h"' hdr_histogram.c
```

The amalgamated `hdr_histogram.c` preprocesses to the exact same translation unit
as the normal multi-file build, so it behaves identically. A CI job
(`.github/workflows/amalgamation.yml`) checks this directory stays in sync with
the sources.
