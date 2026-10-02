# Amalgamated HdrHistogram_c

For projects that embed the library, `script/amalgamate.py` builds a self-contained copy of the
**core** (the record path, the value/percentile read path and the iterators). It has no zlib,
threads, logging-codec, interval-recorder or phaser dependency, only the C standard library and
libm. Each generated `.c` file has its private headers inlined, so there are no private headers
to carry around.

| file | description |
|------|-------------|
| `hdr_histogram.c` | the engine, with the private headers inlined |
| `hdr_histogram.h` | the public API (unchanged copy) |
| `hdr_malloc.h`    | the default libc allocator hook (unchanged copy) |

With `--with-log` it also produces the log codec (needs zlib):

| file | description |
|------|-------------|
| `hdr_histogram_log.c`, `hdr_encoding.c`, `hdr_time.c` | the codec, time helpers and base64/varint code, private headers inlined |
| `hdr_histogram_log.h`, `hdr_time.h` | their public headers |

The output is **generated, not committed**. Each GitHub release has these files attached:
`hdr_histogram-amalgamation-<version>.zip` (core) and
`hdr_histogram-amalgamation-with-log-<version>.zip` (core plus log codec), also as individual
files with a `SHA256SUMS`. They are built by `.github/workflows/release-amalgamation.yml`. This is
the same model SQLite uses for its amalgamation, and it means the files never go stale in the tree.

## Getting it

From a release: download the zip (or the individual files) from the release page.

From any checkout or release tarball:

```sh
script/amalgamate.py --output path/to/dir              # core
script/amalgamate.py --output path/to/dir --with-log   # core + log codec
```

`--output` is relative to the current directory, so you can run the script from an unpacked
release tarball into your own source tree.

## Using it

Copy the files into your project and build `hdr_histogram.c` (and, with `--with-log`, the other
`.c` files) with your own sources:

```sh
cc -c -Ipath/to/amalgamated path/to/amalgamated/hdr_histogram.c
```

### Allocator

To route allocations through your own allocator, provide a header that defines
`hdr_malloc` / `hdr_calloc` / `hdr_realloc` / `hdr_free`. Either compile with it:

```sh
cc -c -DHDR_MALLOC_INCLUDE='"my_alloc.h"' hdr_histogram.c
```

or generate the amalgamation with it already baked in, so no compiler flag is needed:

```sh
script/amalgamate.py --output deps/hdr_histogram --malloc-include my_alloc.h
```

`--malloc-include` only tells the generated files that `my_alloc.h` exists and should be their
default allocator header (one line changes: the default of `HDR_MALLOC_INCLUDE`). The script then
does not write `hdr_malloc.h`, and it does not create `my_alloc.h`: that file is yours, kept next
to the generated ones.

```
deps/hdr_histogram/
  hdr_histogram.c   generated
  hdr_histogram.h   generated
  my_alloc.h        yours
```

A `-DHDR_MALLOC_INCLUDE` on the compiler command line still takes precedence.

### Other switches

`-DHDR_DISABLE_AVX2` also works on the amalgamated file: it drops the runtime AVX2 percentile scan
and always uses the scalar one, exactly as in the full build.

### Header layout

By default the generated `.c` files include the public headers as `"hdr_histogram.h"`, which
suits a flat directory. If your tree keeps them in a `hdr/` directory, as the upstream layout does
(`src/` plus `include/hdr/`), generate with `--include-prefix hdr/`: the `.c` files then include
`"hdr/hdr_histogram.h"`, and the generated `hdr_histogram.h` is byte-identical to the one in
`include/hdr/`.

## Migrating an existing vendored copy

Most embedders carry a flattened copy of the library with a few local edits. This is what it takes
to switch to the generated files. Each recipe below was applied to a copy of the project, which
was then built and tested.

First, check whether your local edits are already upstream:

| local patch | upstream now |
|-------------|--------------|
| `hdr_iter_linear_set_value_units_per_bucket` | yes: `hdr_iter_linear_set_value_units_per_bucket` (a no-op on non-linear iterators) |
| `hdr_record_value_capped` | yes, but it clamps to `[0, highest_trackable_value]`: 0 and values below `lowest_discernible_value` are recorded as they are, instead of being raised |
| an atomic variant of the above | yes: `hdr_record_value_capped_atomic` |
| a `total_count` getter | yes: `hdr_total_count` (NULL-safe, atomic load) |
| `hdr_string_write(&out, h)` | yes: `hdr_log_encode(h, &out)` (same body, arguments swapped) |
| the struct field `lowest_trackable_value` | renamed upstream to `lowest_discernible_value` |

Then the mechanical steps:

* **Redis** (`deps/hdr_histogram`, built with `-std=c99 -Wall -Os`): run
  `script/amalgamate.py --output deps/hdr_histogram --malloc-include hdr_redis_malloc.h`, then delete
  `hdr_atomic.h` and `hdr_tests.h`. Keep your `hdr_redis_malloc.h`. The `Makefile` needs no change
  (its `-DHDR_MALLOC_INCLUDE` is now redundant but harmless). The iterator patch can be dropped.
* **Valkey**: the same command with your shim's name. Valkey also builds the dependency with CMake,
  and its `CMakeLists.txt` lists `hdr_atomic.h` among the sources: remove that one entry, or CMake
  stops with "Cannot find source file".
* **memtier_benchmark** uses the log codec and the time helpers as well, so generate with
  `--with-log` (no `--malloc-include` needed, it will use `hdr_malloc.h`):
  `script/amalgamate.py --output deps/hdr_histogram --with-log`. Delete the other files in that
  directory (`hdr_atomic.h`, `hdr_encoding.h`, `hdr_endian.h`, `byteorder.h`) and list the new ones
  in `Makefile.am`: `hdr_histogram.c/.h`, `hdr_histogram_log.c/.h`, `hdr_encoding.c`, `hdr_time.c/.h`,
  `hdr_malloc.h`. Then drop the local `hdr_record_value_capped_atomic`, rename
  `lowest_trackable_value` to `lowest_discernible_value`, and replace `hdr_string_write` as above.
  The memtier includes (`"deps/hdr_histogram/hdr_histogram.h"`) are unchanged.
* **Node.js** (`deps/histogram`, which keeps `src/` plus `include/hdr/`): generate with
  `script/amalgamate.py --output <tmp> --include-prefix hdr/`, copy `hdr_histogram.c` to
  `src/hdr_histogram.c` and `hdr_histogram.h` to `include/hdr/hdr_histogram.h`, and delete
  `src/hdr_atomic.h` and `src/hdr_tests.h`. `histogram.gyp`, `BUILD.gn` and `unofficial.gni` list only
  those two files, so they need no change.

One behaviour change to know about when upgrading from an older copy: bucket counts are now
required to be non-negative. The record functions reject a negative count, and decoding a log
with a negative bucket count fails with `HDR_NEGATIVE_COUNT_INVALID`. Code that writes `counts[]`
directly should not store negative values.

## How it stays correct

Each amalgamated `.c` file preprocesses to the exact same translation unit as the normal
multi-file build, so it behaves identically. CI (`.github/workflows/amalgamation.yml`) generates
every variant on each change, checks that equivalence, builds with `-Wall -Wextra -Werror`, and
round-trips a histogram through the amalgamated log codec. The release job runs the same checks
before it attaches anything. You can run them by hand with `sh script/check-amalgamation.sh`.
