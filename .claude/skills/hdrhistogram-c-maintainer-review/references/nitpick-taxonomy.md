# Cross-cutting nitpick taxonomy — HdrHistogram_c, real precedent only

Grounded in actual GitHub history on `HdrHistogram/HdrHistogram_c` (70 merged PRs across 11 years, 59 issues
sampled, as of September 2026), plus the project's own `README.md`, `CMakeLists.txt`, `.github/workflows/ci.yml`,
`.github/workflows/cflite_pr.yml`/`cflite_batch.yml`, and header comments in `include/hdr/hdr_histogram.h`. This
is a widely-embedded, ABI/portability-sensitive numeric C library — the categories below are real, evidenced
recurring concern classes, not a generic C-review checklist imported from elsewhere.

## 1. Undefined behavior, integer overflow, and precision — a real, recurring, and inconsistently-resolved class

- **Left-shift of a negative value (UB), caught by clang's `-fsanitize=undefined,address`**: issue #123
  (`TimWolla`), a precise repro (encoding an empty histogram) with sanitizer output attached. Real, fixed —
  mikeb01's entire reply: *"Fixed."*
- **`INTEGER_OVERFLOW` in `hdr_reset_internal_counters`**: issue #118, filed with exact `hdr_histogram.c` line
  citations (both operands and the summation are `int64_t` with no overflow check). Real, **still open and
  unaddressed** as of this mining — do not assume every real, well-documented correctness report here gets
  fixed promptly.
- **A GCC 12 `ipa-ra` misoptimization when linking the static lib into a `.so`**: issue #124, an unusually
  deep, well-instrumented report (`TimWolla` again) with a minimal repro via `dlopen`/`dlsym`. Real, **open
  with zero comments** for well over a year as of this mining — genuine evidence that even a serious,
  well-documented report can go unanswered here if it's hard to verify/reproduce.
- **Reading past the end of an input buffer**: PR #93 (`uluyol`), merged with no recorded review comment —
  real precedent that a memory-safety fix to the decode path is treated as self-evidently correct and applied
  without discussion once the diff is clearly a bounds fix.

**When reviewing:** treat any new arithmetic on a caller-controlled or file-derived `int64_t`/`int32_t` value
(shift, sum, multiply, subtraction that could go negative) as worth a bounds/overflow question, citing #118 and
#123 as real, non-hypothetical precedent — but calibrate confidence honestly: this project's own open issues
show these are real, live gaps, not settled invariants the codebase already guards against everywhere.

## 2. Cross-platform build/portability — the single most evidenced recurring category

Real, merged fixes and real, filed reports spanning the library's entire life, each on a platform mikeb01
personally could not test:

- FreeBSD 13.2 (`hdr_endian.h` macro redefinition against `<sys/_endian.h>`) — issue #119, fixed.
- HardenedBSD (same underlying endian-macro clash, blocking a Node.js 24 build via `ld-elf.so.1` link
  failure) — issue #131, fixed via community-supplied, community-tested patches (`lattera`, `netchild`);
  mikeb01's own words: *"I've applied these patches. I don't have any BSDs install locally to test with
  unfortunately."*
- OpenBSD — PR #77 (`ohz10`), the PR body itself states real local test coverage (*"Tested on OpenBSD 6.6 with
  clang 8.0.1 and gcc 8.3.0... All tests in the test/ folder pass"*) as the actual verification, since CI
  doesn't cover this platform.
- musl/Alpine Linux — issue #79, a non-POSIX `getline` usage broke under musl; mikeb01's fix names the
  exact commit SHA and root cause (*"The getline function we were using was non-posix and support probably
  differed between libc and musl"*).
- AIX (#34), ARM/Raspberry Pi (#98), MSVC ClangCL x64 (#142) and 32-bit i386 (#143) AVX2-dispatch build
  hazards — all real, filed or fixed.
- `ci.yml`'s own matrix is itself real, direct evidence of how seriously this is taken: Linux/Windows/macOS ×
  x86/x64 × Debug/RelWithDebInfo × two CMake versions × `HDR_LOG_REQUIRED` ON/DISABLED — but note its real
  gaps too: **no BSD runner, no musl/Alpine runner, no 32-bit Linux runner, and no `-Werror`** (the `-Wall
  -Wextra -Wshadow -Winit-self -Wpedantic -Wmissing-prototypes` flags in `CMakeLists.txt` are visible in build
  logs but not build-breaking). A portability or warning-flag claim that ci.yml would have already caught
  should be checked against what the matrix *actually* covers, not assumed.

**When reviewing:** a build/portability fix touching endian macros, `getline`/POSIX assumptions, or
compiler-intrinsic availability (`__builtin_cpu_supports`, `_mm_extract_epi64` and similar AVX2/SSE
intrinsics) is a real, high-value category here — and because CI can't test most of the platforms this
actually breaks on, the PR author's own stated verification (what they actually built/ran, and where) matters
more than it would in a project with full CI coverage.

## 3. CMake/build-system/packaging — real, externally-driven, and design-conscious

- **zlib dependency**: `HDR_LOG_REQUIRED` gates whether `hdr_histogram_log.c` (needs zlib) or
  `hdr_histogram_log_no_op.c` is compiled in — a real, documented escape hatch for environments without zlib
  (issue #113: mikeb01's fix was literally telling the reporter to flip this option).
  `HDR_HISTOGRAM_BUILD_PROGRAMS` similarly gates whether tests/examples build at all.
- **pkg-config correctness**: a real, multi-PR cluster (#103 add pkg-config file, #104/#105 fix/clean its
  installation, #106 fix variable-relative-path handling) — small, easy-to-get-wrong details in a `.pc` file
  that real downstream packagers actually hit.
- **Install layout / header mirroring**: #97 (conflicting static+shared install options), #100/#101/#102
  (mirror `include/hdr/` layout into the source tree so `#include "hdr/hdr_histogram.h"` and installed-header
  paths agree), #127/#128 (headers not found when installed to a non-default prefix; a CMake `ALIAS` target
  for subproject use) — all real, all from people actually packaging or vendoring this library.
- **SOVERSION is deliberately decoupled from the project version** — see item 4 below; this is itself a real,
  externally-caught packaging bug (issue #31), not an assumption.

**When reviewing:** a CMake change should be checked against whether it preserves these documented options
(`HDR_LOG_REQUIRED`, `HDR_HISTOGRAM_BUILD_PROGRAMS`), whether it keeps the public/private header split
(`HDR_HISTOGRAM_PUBLIC_HEADERS` vs. `HDR_HISTOGRAM_PRIVATE_HEADERS` in `src/CMakeLists.txt`) intact, and
whether it touches install paths or the `.pc` file in a way a real downstream packager (per the real history
above) would notice.

## 4. ABI stability — written process, born from a real incident

`CMakeLists.txt` documents a concrete, 4-step SOVERSION-calculation process immediately above
`HDR_SOVERSION_CURRENT`/`_REVISION`/`_AGE`, explicitly marked *"NOTE: THIS IS UNRELATED to the actual project
version."* This process exists because of a real, evidenced incident: issue #31 (`remicollet`, a real distro
packager), *"SOVERSION is not to be related to library version"* — with a concrete libsodium comparison table
as evidence the two should be decoupled — and it was fixed. Separately, mikeb01's own words on a real bug fix
(issue #36): *"I'll need to check ABI compatibility before I push out a new release, but the head of the git
repo has the fix and is stable if you need it immediately"* — real, direct evidence that a merged fix and an
ABI-safe *release* are two different things he tracks separately. The still-open "Road to 1.0" issue (#95,
opened by mikeb01 himself) is itself about this: packed arrays, resizable counts, and double-histogram support
are all blocked on an intentional, not-yet-taken decision to break ABI by making `struct hdr_histogram` opaque,
specifically because the struct is currently defined in the public header (`include/hdr/hdr_histogram.h`).

**When reviewing:** any change to the layout of `struct hdr_histogram` (in the public header), the signature
of an existing public function, or anything that would change what symbols/sizes a compiled consumer depends
on is an ABI question, not just a code-review question — check whether the PR touches
`HDR_SOVERSION_CURRENT`/`_REVISION`/`_AGE` per the documented 4-step process, and flag it plainly if a
struct/signature change looks ABI-breaking but the SOVERSION wasn't bumped accordingly.

## 5. Memory safety — leaks and corruption, both real and both eventually fixed

- **`hdr_close()` didn't exist; callers leaked the histogram** — PR #52 (`chronoxor`), fixed; mikeb01: *"Thank
  you for the patch."* Later hardened to be a safe no-op on a null pointer (PR #75).
- **`hdr_interval_recorder` leak** — PR #58 (`markaylett`), fixed, no recorded discussion.
- **3 real memory leaks in the log-decode error paths, found by the fuzzer the very next PR after fuzzing was
  added** — PR #122 (`DavidKorczynski`), immediately following PR #120 (adding ClusterFuzzLite itself): the
  decode functions called `hdr_init` then, on a later failure, cleaned up with `hdr_free(h)` instead of
  `hdr_close(h)`, leaking the `counts` array. Real, concrete, same-day evidence the fuzzing infrastructure
  finds real bugs immediately once it exists.
- **Under-allocation in `hdr_encode_compressed()` causing real memory corruption** on short/empty
  histograms — issue #18 (`ahothan`), a precise diff-form report.

**When reviewing:** any change to an error/cleanup path in `hdr_histogram_log.c`'s decode functions, or any
new allocation paired with a `hdr_init`/`hdr_close`/`hdr_free`-style teardown, is a real, evidenced risk area —
specifically check that every allocated field is freed on *every* early-return path, not just the success path,
per the exact real bug PR #122 fixed.

## 6. Percentile/statistical correctness — cross-checked against the Java reference implementation

- **Empty-histogram percentile behavior is genuinely surprising and unresolved**: issue #116 —
  `hdr_value_at_percentile` on an empty histogram returns a non-obvious value (63, not 0); mikeb01's own
  reply concedes this needs checking against Java's behavior, not a confident "working as intended."
- **`hdr_min()`/`hdr_max()` vs. the `min_value`/`max_value` struct fields are semantically different, and this
  has confused a real user**: issue #130 — `hdr_max()` returns the *highest value equivalent to the recorded
  bucket*, not the literal recorded maximum; mikeb01's fix was clarifying documentation, with a direct citation
  of the equivalent line in the upstream Java `AbstractHistogram.java`.
- **`hdr_record_value` can record out-of-bounds values without capping**, a real, actively-worked-on gap:
  issue #126, filed with concrete provenance (real `memtier_benchmark` NaN/Inf bugs it caused downstream,
  issues #271/#272 there). As of this mining, mikeb01 has added the upper-bound check but the lower bound is
  explicitly still open, pending comparison with the Java implementation.

**When reviewing:** any change to percentile calculation, min/max tracking, or bounds-checking on record
should be checked against what the upstream Java HdrHistogram actually does for the same input — that is the
real, evidenced standard this maintainer applies to these questions, not house intuition. If you can't verify
the Java behavior, say so plainly rather than asserting a "correct" answer.

## 7. Public API convention: the caller owns allocation, not the callee

The one real, evidenced design pushback in this repo's history: PR #90, `filipecosta90` proposed a new
`hdr_value_at_percentiles()` API that allocated its own output array internally; mikeb01: *"I'm happy to
include this, but I think the `values` array should be allocated by the caller and not inside the function."*
The contributor complied in the same PR. Any new public function that would allocate and hand back a
caller-visible buffer (as opposed to filling a caller-supplied one) should be checked against this real,
established convention.

## 8. Fuzzing (`cflite_pr.yml`/`cflite_batch.yml`) is a real, working second reviewer — with a real gap

`cflite_pr.yml` runs ClusterFuzzLite with **AddressSanitizer only**, 200 seconds, on every PR against `main`.
`cflite_batch.yml` runs a much deeper weekly (Monday 03:00 UTC) or manually-triggered batch across **both
AddressSanitizer and UndefinedBehaviorSanitizer**, up to an hour each. This is real and already caught real
bugs (item 5 above, PR #122's leaks, found the PR immediately after fuzzing was added). **The real gap**: a
per-PR run does not include UBSan — issue #123's real UB bug (left-shift of a negative value) was originally
found by a contributor building locally with `-fsanitize=undefined,address`, not by CI, because the per-PR
fuzzing gate doesn't run UBSan; only the weekly batch job does. Cite the per-PR fuzz gate as real, working
coverage for memory-safety bugs (leaks, overflows, use-after-free) reachable by the existing fuzz targets, but
don't claim it covers UB the way the weekly batch job does, and don't assume either one exercises a code path
with no corresponding fuzz target.

## What this taxonomy is honestly thin or silent on

- **A CONTRIBUTING.md, CODEOWNERS, or written test-coverage policy.** None exist in this repo. Don't cite a
  house rule that isn't written down anywhere real — reason from the real PR/issue precedent above instead.
- **Style/formatting nits beyond what `-Wall -Wextra -Wshadow -Winit-self -Wpedantic -Wmissing-prototypes`
  would already show in a build log.** These flags are real and present in `CMakeLists.txt`, but **not**
  `-Werror` — a warning is visible, not build-breaking. No real reviewer comment in the sampled history
  re-litigates a style nit CI's build log would already surface; don't manufacture one.
- **Test coverage as a literal enforced gate.** There's no coverage tool wired into CI at all (no Codecov,
  no `%`-based gate) — new tests get added when a bug fix naturally warrants one (the real pattern across
  nearly every merged bug-fix PR sampled), but there's no evidence of a PR being blocked purely on missing
  coverage. State what the diff actually tests; don't invent a coverage percentage requirement.
- **A dense, back-and-forth multi-person review thread.** The real record here is one-line
  merge-and-acknowledge from mikeb01, with the two `filipecosta90` PRs (#142/#143) as the only real exception
  — see `voice-profiles.md`'s closing section for how to calibrate that honestly.
