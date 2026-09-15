#!/bin/bash -eu
# Use the following environment variables to build the code
# $CXX:               c++ compiler
# $CC:                c compiler
# CFLAGS:             compiler flags for C files
# CXXFLAGS:           compiler flags for CPP files
# LIB_FUZZING_ENGINE: linker flag for fuzzing harnesses

mkdir build
cd build
cmake ../
make

# Build and copy all fuzzer executables to $OUT/. Each links the static
# library and zlib (used by the encode/decode paths).
for fuzzer in log_reader_fuzzer hdr_record_fuzzer hdr_decode_fuzzer; do
  $CC $CFLAGS $LIB_FUZZING_ENGINE \
    $SRC/hdrhistogram_c/.clusterfuzzlite/${fuzzer}.c \
    -o $OUT/${fuzzer} \
    -I$SRC/hdrhistogram_c/include \
    $SRC/hdrhistogram_c/build/src/libhdr_histogram_static.a -l:libz.a
done

# Prepare corpus. The sample .hlog logs seed the log-parsing entry points.
zip -j $OUT/log_reader_fuzzer_seed_corpus.zip $SRC/hdrhistogram_c/test/*.hlog
