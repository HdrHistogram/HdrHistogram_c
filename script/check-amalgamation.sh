#!/bin/sh
# Check the optional amalgamation variants (see docs/AMALGAMATION.md):
#   --with-log          the log codec files, each with its private headers inlined
#   --include-prefix    a tree that keeps the public headers in a hdr/ directory
#
# Run from the repository root. Needs a C compiler, python3 and zlib.
# Everything is written under the directory given as $1 (default: a temp dir).
set -eu

CC=${CC:-cc}
WORK=${1:-$(mktemp -d)}
mkdir -p "$WORK"
SRC_FLAGS="-Iinclude -Isrc"
CFLAGS="-std=c99 -Wall -Wextra -Wmissing-prototypes -Os -D_GNU_SOURCE"

same_tu() { # $1 = file name, $2 = amalgamated directory
    $CC -E -P $SRC_FLAGS "src/$1" | grep -v '^[[:space:]]*$' > "$WORK/orig.txt"
    $CC -E -P -I"$2" "$2/$1" | grep -v '^[[:space:]]*$' > "$WORK/amal.txt"
    cmp "$WORK/orig.txt" "$WORK/amal.txt"
    echo "  $1: preprocesses to the same translation unit as src/$1"
}

echo "== --with-log"
python3 script/amalgamate.py --output "$WORK/log" --with-log
for f in hdr_histogram.c hdr_histogram_log.c hdr_encoding.c hdr_time.c; do
    same_tu "$f" "$WORK/log"
    $CC $CFLAGS -Werror -I"$WORK/log" -c "$WORK/log/$f" -o "$WORK/log/$f.o"
done
# no private header, and no <hdr/...> include, may be left behind
if grep -nE '#[[:space:]]*include[[:space:]]*[<"](hdr/|hdr_tests|hdr_endian|hdr_encoding|hdr_atomic|hdr_histogram_internal)' \
        "$WORK"/log/*.c "$WORK"/log/*.h; then
    echo "ERROR: unresolved private or hdr/ include in the --with-log output" >&2
    exit 1
fi

cat > "$WORK/roundtrip.c" <<'EOF'
#include "hdr_histogram.h"
#include "hdr_histogram_log.h"
#include <stdlib.h>
#include <string.h>
int main(void)
{
    struct hdr_histogram *h = NULL, *d = NULL;
    char *enc = NULL;
    int i, ok;
    hdr_timespec ts;
    if (hdr_init(1, 3600000000LL, 3, &h)) return 1;
    for (i = 1; i <= 50000; i++) hdr_record_value(h, (int64_t) i * 37 % 100000 + 1);
    if (hdr_log_encode(h, &enc)) return 2;
    if (hdr_log_decode(&d, enc, strlen(enc))) return 3;
    hdr_gettime(&ts);
    ok = h->total_count == d->total_count &&
         hdr_value_at_percentile(h, 99) == hdr_value_at_percentile(d, 99) && ts.tv_sec > 0;
    hdr_close(h); hdr_close(d); free(enc);
    return ok ? 0 : 4;
}
EOF
$CC $CFLAGS -I"$WORK/log" "$WORK/roundtrip.c" "$WORK/log/hdr_histogram.c" "$WORK/log/hdr_histogram_log.c" \
    "$WORK/log/hdr_encoding.c" "$WORK/log/hdr_time.c" -lz -lm -o "$WORK/roundtrip"
"$WORK/roundtrip"
echo "  log codec round trip through the amalgamated files: ok"

echo "== --include-prefix hdr/ (public headers kept in a hdr/ directory)"
python3 script/amalgamate.py --output "$WORK/gen" --include-prefix hdr/
mkdir -p "$WORK/tree/src" "$WORK/tree/include/hdr"
cp "$WORK/gen/hdr_histogram.c" "$WORK/gen/hdr_malloc.h" "$WORK/tree/src/"
cp "$WORK/gen/hdr_histogram.h" "$WORK/tree/include/hdr/"
cmp "$WORK/tree/include/hdr/hdr_histogram.h" include/hdr/hdr_histogram.h
echo "  hdr_histogram.h is byte-identical to include/hdr/hdr_histogram.h"
$CC $CFLAGS -Werror -I"$WORK/tree/src" -I"$WORK/tree/include" -c "$WORK/tree/src/hdr_histogram.c" -o "$WORK/tree/hdr_histogram.o"
echo "  compiles with -Isrc -Iinclude"

echo "ok"
