FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y make autoconf automake libtool zlib1g-dev

COPY . $SRC/hdrhistogram_c
COPY .clusterfuzzlite/build.sh $SRC/build.sh
WORKDIR $SRC/hdrhistogram_c
