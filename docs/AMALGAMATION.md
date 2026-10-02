# Amalgamated HdrHistogram_c core

For projects that embed the library, `script/amalgamate.py` builds a self-contained
copy of the **core** (the record path, the value/percentile read path and the
iterators). It has no zlib, threads, logging-codec, interval-recorder or phaser
dependency, only the C standard library and libm.

| file | description |
|------|-------------|
| `hdr_histogram.c` | the engine, with the private headers inlined |
| `hdr_histogram.h` | the public API (unchanged copy) |
| `hdr_malloc.h`    | the default libc allocator hook (unchanged copy) |

The output is **generated, not committed**. Each GitHub release has these files attached
(`hdr_histogram-amalgamation-<version>.zip` and the three files on their own), built by
`.github/workflows/release-amalgamation.yml`. This is the same model SQLite uses for its
amalgamation, and it means the files never go stale in the tree.

## Getting it

From a release: download the zip (or the individual files) from the release page.

From any checkout or release tarball:

```sh
script/amalgamate.py --output path/to/dir
```

`--output` is relative to the current directory, so you can run the script from an unpacked
release tarball into your own source tree.

## Using it

Copy the files into your project and build `hdr_histogram.c` with your own sources:

```sh
cc -c -Ipath/to/amalgamated path/to/amalgamated/hdr_histogram.c
```

To route allocations through your own allocator, provide a header that defines
`hdr_malloc` / `hdr_calloc` / `hdr_realloc` / `hdr_free`. Either compile with it:

```sh
cc -c -DHDR_MALLOC_INCLUDE='"my_alloc.h"' hdr_histogram.c
```

or generate the amalgamation with it already baked in, so no compiler flag is needed:

```sh
script/amalgamate.py --output deps/hdr_histogram --malloc-include my_alloc.h
```

`--malloc-include` only tells the generated `hdr_histogram.c` that `my_alloc.h` exists and
should be its default allocator header (one line changes: the default of
`HDR_MALLOC_INCLUDE`). The script then writes just two files, `hdr_histogram.c` and
`hdr_histogram.h`. It does not write `hdr_malloc.h`, and it does not create `my_alloc.h`:
that file is yours, kept next to the generated ones.

```
deps/hdr_histogram/
  hdr_histogram.c   generated
  hdr_histogram.h   generated
  my_alloc.h        yours
```

A `-DHDR_MALLOC_INCLUDE` on the compiler command line still takes precedence.

## How it stays correct

The amalgamated `hdr_histogram.c` preprocesses to the exact same translation unit as the
normal multi-file build, so it behaves identically. CI (`.github/workflows/amalgamation.yml`)
generates it on every change and checks that, so the script cannot drift from the sources.
