# Embedding HdrHistogram_c

Notes for projects that embed HdrHistogram_c (statically link it, or vendor its
sources) rather than consuming a system-installed shared library.

## Minimal static core

`hdr_histogram.c` is self-contained: it implements the record path
(`hdr_record_value`), the read path (`hdr_value_at_percentile[s]`), the
iterators and reset, and depends only on the C standard library and `libm`. The
logging codec, interval recorder, reader/writer phaser and threading helpers are
separate translation units and are *not* needed to record and query values.

To build just that core as a static archive:

```sh
cmake -S . -B build/core -DHDR_HISTOGRAM_CORE_ONLY=ON
cmake --build build/core
```

`-DHDR_HISTOGRAM_CORE_ONLY=ON` produces a single target,
`hdr_histogram_core_static` (`libhdr_histogram_core_static.a`), and:

* does **not** run `find_package(ZLIB)` or `find_package(Threads)`, so a
  core-only build has no zlib or pthread dependency to satisfy;
* skips the logging/recorder/phaser sources, the tests, examples, benchmarks and
  the CMake package/pkg-config files;
* installs the core archive plus the public headers.

Link a consumer against the core archive and `libm` only:

```sh
cc my_app.c -Iinclude build/core/src/libhdr_histogram_core_static.a -lm
```

Only `hdr/hdr_histogram.h` is backed by the core archive. The logging
(`hdr_histogram_log.h`), recorder and phaser headers are still installed for
reference but their symbols live in the full library; build without
`HDR_HISTOGRAM_CORE_ONLY` if you need them.

The full default build (shared + static libraries, logging, recorder, installed
CMake/pkg-config files, ABI/SONAME) is unchanged when this option is off.

### Vendoring the source directly

If you compile the sources into your own build rather than using CMake, the core
is just one file:

```sh
cc -c -Iinclude src/hdr_histogram.c
```

## Custom allocator

All allocations go through the `hdr_malloc` / `hdr_calloc` / `hdr_realloc` /
`hdr_free` macros in `src/hdr_malloc.h`, which default to the libc allocator.
Point `HDR_MALLOC_INCLUDE` at a header that redefines them to route allocations
through your own allocator; this works for both the full library and the core.

## Disabling the AVX2 percentile scan

The percentile scan has a runtime-dispatched AVX2 implementation on x86-64
(GCC/Clang) with a scalar fallback; the binary never *requires* AVX2 because the
path is selected at runtime via `__builtin_cpu_supports`. To drop the AVX2 code
entirely and always use the scalar scan — for reproducible/deterministic builds,
to match non-x86 codegen, or to shrink the object — configure with:

```sh
cmake -S . -B build -DHDR_HISTOGRAM_DISABLE_AVX2=ON
```

This defines `HDR_DISABLE_AVX2` for the build. The same macro can be defined
directly (`-DHDR_DISABLE_AVX2`) when vendoring the source without CMake. It is
independent of `HDR_HISTOGRAM_CORE_ONLY` and applies to every build mode.
