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

# Build and copy fuzzer executables to $OUT/
$CC $CFLAGS $LIB_FUZZING_ENGINE \
  $SRC/hdrhistogram_c/.clusterfuzzlite/log_reader_fuzzer.c \
  -o $OUT/log_reader_fuzzer \
  -I$SRC/hdrhistogram_c/include \
  $SRC/hdrhistogram_c/build/src/libhdr_histogram_static.a -l:libz.a

# Prepare corpus
zip -j $OUT/log_reader_fuzzer_seed_corpus.zip $SRC/hdrhistogram_c/test/*.hlog
