#!/usr/bin/env python3
"""Amalgamate the HdrHistogram_c core into a self-contained drop-in.

The core (record path + value/percentile read path + iterators) lives in
``src/hdr_histogram.c`` and depends only on the C standard library and libm --
no zlib, threads, logging codec, interval recorder or phaser. This script emits
it as a couple of files that an embedding project can vendor instead of tracking
the whole source tree. The output is generated, not committed: it is attached to
each GitHub release, and can be regenerated from any checkout or release tarball.

  <out>/hdr_histogram.c   the engine, with the private headers (hdr_atomic.h,
                          hdr_tests.h) inlined
  <out>/hdr_histogram.h   the public API (already self-contained; copied as-is)
  <out>/hdr_malloc.h      the default libc allocator hook (copied as-is; not written
                          with --malloc-include)

--with-log also emits the log codec, for projects that use the hdr_log_* API (it
needs zlib and the core above). Each file has its private headers inlined, so no
private header is ever needed:

  <out>/hdr_histogram_log.c  <out>/hdr_encoding.c  <out>/hdr_time.c
  <out>/hdr_histogram_log.h  <out>/hdr_time.h

Override the allocator exactly as for the normal build, by compiling with
  -DHDR_MALLOC_INCLUDE='"my_alloc.h"'
or bake it in with --malloc-include, which makes my_alloc.h the default instead
of hdr_malloc.h. hdr_malloc.h is then not written, and my_alloc.h is not created
(it is yours, kept alongside the output). A -DHDR_MALLOC_INCLUDE on the compiler
command line still wins.

--include-prefix hdr/ is for trees that keep the public headers in a hdr/
directory (as the upstream layout does): the generated .c files then include
"hdr/hdr_histogram.h" instead of "hdr_histogram.h". The default is no prefix.

--output is relative to the current directory, so the script can be run from a
release tarball into your own tree.

Usage:
  script/amalgamate.py --output DIR
  script/amalgamate.py --output deps/hdr_histogram --malloc-include hdr_redis_malloc.h
  script/amalgamate.py --output deps/hdr_histogram --with-log
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Translation units and the search path for their local (private) includes.
CORE_TUS = ["src/hdr_histogram.c"]
LOG_TUS = ["src/hdr_histogram_log.c", "src/hdr_encoding.c", "src/hdr_time.c"]
INCLUDE_DIRS = ["src", "include/hdr"]

# Public headers are shipped alongside and referenced by name, never inlined.
PUBLIC_HEADERS = {
    "hdr_histogram.h": "include/hdr/hdr_histogram.h",
    "hdr_histogram_log.h": "include/hdr/hdr_histogram_log.h",
    "hdr_time.h": "include/hdr/hdr_time.h",
}
CORE_HEADERS = ["hdr_histogram.h"]
LOG_HEADERS = ["hdr_histogram_log.h", "hdr_time.h"]
MALLOC_HEADER = "hdr_malloc.h"

MALLOC_DEFAULT = '#define HDR_MALLOC_INCLUDE "hdr_malloc.h"'
HEADER_NAME_RE = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_./+-]*$')
PREFIX_RE = re.compile(r'^([A-Za-z0-9_][A-Za-z0-9_.+-]*/)*$')

# Matches  #include "foo.h"  or  #include <hdr/foo.h>  (not #include SOME_MACRO).
# A trailing /* ... */ comment is allowed (it is dropped when the header is inlined).
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*(?:"([^"]+)"|<([^>]+)>)\s*(?:/\*.*\*/\s*)?$')
HDR_ANGLE_RE = re.compile(r'^(\s*#\s*include\s*)<hdr/([A-Za-z0-9_]+\.h)>(.*)$')


def version():
    path = os.path.join(ROOT, "include/hdr/hdr_histogram_version.h")
    try:
        with open(path) as f:
            m = re.search(r'#define\s+HDR_HISTOGRAM_VERSION\s+"([^"]+)"', f.read())
            if m:
                return m.group(1)
    except OSError:
        pass
    return "unknown"


def resolve(name, angle):
    """Return the repo-relative path of a local header, or None if not local.

    Quoted includes are project-local. Angle includes are system headers unless
    they are <hdr/...>, so <foo.h> is never inlined just because src/ has a foo.h.
    """
    if angle and not name.startswith("hdr/"):
        return None
    base = os.path.basename(name)
    for d in INCLUDE_DIRS:
        p = os.path.join(d, base)
        if os.path.isfile(os.path.join(ROOT, p)):
            return p
    return None


def inline(path, seen, out, prefix):
    """Append `path`'s content to `out`, recursively inlining private includes."""
    with open(os.path.join(ROOT, path)) as f:
        lines = f.readlines()
    out.append("/* ---- begin %s ---- */\n" % path)
    for line in lines:
        m = INCLUDE_RE.match(line)
        if m:
            name = m.group(1) or m.group(2)
            angle = m.group(1) is None
            base = os.path.basename(name)
            if base in PUBLIC_HEADERS and (not angle or name.startswith("hdr/")):
                if base not in seen:
                    seen.add(base)
                    out.append('#include "%s%s"\n' % (prefix, base))
                continue
            target = resolve(name, angle)
            if target is not None:
                if target not in seen:
                    seen.add(target)
                    inline(target, seen, out, prefix)
                continue
        # system include, the HDR_MALLOC_INCLUDE macro hook, or plain code
        out.append(line)
    out.append("/* ---- end %s ---- */\n" % path)


def generate(tu, malloc_include, prefix):
    name = os.path.basename(tu)
    kind = "core" if tu in CORE_TUS else "log codec"
    banner = (
        "/*\n"
        " * HdrHistogram_c %s -- amalgamated (%s, private headers inlined).\n"
        " *\n"
        " * GENERATED by script/amalgamate.py from HdrHistogram_c %s. DO NOT EDIT;\n"
        " * change the sources and regenerate. Public domain (see hdr_histogram.h).\n"
        " */\n" % (kind, name, version())
    )
    if tu in CORE_TUS:  # keep the core banner exactly as it has always been
        banner = (
            "/*\n"
            " * HdrHistogram_c core -- amalgamated single-file build.\n"
            " *\n"
            " * GENERATED by script/amalgamate.py from HdrHistogram_c %s. DO NOT EDIT;\n"
            " * change the sources and regenerate. Public domain (see hdr_histogram.h).\n"
            " */\n" % version()
        )
    out = [banner]
    # Empty seen-set: each public header include is emitted once, at its natural
    # position; later references (e.g. from hdr_tests.h) are deduped.
    inline(tu, set(), out, prefix)
    text = "".join(out)
    if malloc_include and MALLOC_DEFAULT in text:
        if text.count(MALLOC_DEFAULT) != 1:
            sys.exit("unexpected HDR_MALLOC_INCLUDE default in %s" % tu)
        text = text.replace(
            MALLOC_DEFAULT, '#define HDR_MALLOC_INCLUDE "%s"' % malloc_include)
    return text


def public_header(name, prefix):
    """Copy a public header; point its <hdr/...> includes at the chosen layout."""
    with open(os.path.join(ROOT, PUBLIC_HEADERS[name])) as f:
        lines = f.readlines()
    if prefix == "hdr/":
        return "".join(lines)  # the upstream layout: verbatim
    out = []
    for line in lines:
        m = HDR_ANGLE_RE.match(line)
        if m and m.group(2) in PUBLIC_HEADERS:
            line = '%s"%s%s"%s\n' % (m.group(1), prefix, m.group(2), m.group(3))
        out.append(line)
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--output", metavar="DIR", required=True,
                    help="write the amalgamation to DIR (relative to the current directory)")
    ap.add_argument("--malloc-include", metavar="HEADER",
                    help="make HEADER the default allocator header instead of hdr_malloc.h "
                         "(you supply it; e.g. hdr_redis_malloc.h)")
    ap.add_argument("--with-log", action="store_true",
                    help="also emit the log codec (hdr_histogram_log.c, hdr_encoding.c, "
                         "hdr_time.c and their public headers); needs zlib")
    ap.add_argument("--include-prefix", metavar="DIR/", default="",
                    help='how the generated files include the public headers, e.g. "hdr/" '
                         "for a tree that keeps them in a hdr/ directory (default: none)")
    args = ap.parse_args()
    if args.malloc_include is not None and not HEADER_NAME_RE.match(args.malloc_include):
        ap.error("--malloc-include must be a plain header file name")
    if not PREFIX_RE.match(args.include_prefix):
        ap.error('--include-prefix must be empty or a relative directory ending in "/"')

    tus = CORE_TUS + (LOG_TUS if args.with_log else [])
    headers = CORE_HEADERS + (LOG_HEADERS if args.with_log else [])
    files = {}
    for tu in tus:
        files[os.path.basename(tu)] = generate(tu, args.malloc_include, args.include_prefix)
    if args.malloc_include and ('"%s"' % args.malloc_include) not in files["hdr_histogram.c"]:
        sys.exit("cannot find the HDR_MALLOC_INCLUDE default in src/hdr_histogram.c")
    for h in headers:
        files[h] = public_header(h, args.include_prefix)
    if not args.malloc_include:
        with open(os.path.join(ROOT, "src", MALLOC_HEADER)) as f:
            files[MALLOC_HEADER] = f.read()

    dst = os.path.abspath(args.output)
    os.makedirs(dst, exist_ok=True)
    for name, content in files.items():
        with open(os.path.join(dst, name), "w") as f:
            f.write(content)
    print("wrote %d files to %s" % (len(files), args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
