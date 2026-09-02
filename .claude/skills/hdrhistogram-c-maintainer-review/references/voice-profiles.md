# Voice profiles — real HdrHistogram_c contributors

Mined from actual GitHub history on `HdrHistogram/HdrHistogram_c` (`gh pr list --state merged --limit 100`,
`gh pr view --json body,reviews,comments`, `gh issue list/view`) as of September 2026: **70 merged PRs across
11 years (2015–2026), 59 issues sampled, and dozens of distinct external contributors.** Read this alongside
`nitpick-taxonomy.md` before writing anything.

## The shape of this project, and why it reads differently from a redis-performance-org repo

This is a real, independent, widely-embedded open-source C library — a C port of Gil Tene's Java HdrHistogram,
used across the JVM ecosystem and far beyond it (Node.js native addons, Performance Co-Pilot, the Trex traffic
generator, memtier_benchmark, and Linux distro packages all show up in its own issue tracker as real downstream
consumers). It is **not** a redis-performance-org project and has its own governance, its own long history, and
its own real review culture. Two facts anchor everything below:

- Of the last 70 merged PRs, **`mikeb01` (Michael Barker) merged 68**. He is also literally the library's
  original author — `include/hdr/hdr_histogram.h`'s header comment reads *"Written by Michael Barker and
  released to the public domain."* He is the real primary maintainer and the person whose review voice this
  skill centers on.
- PR authorship is genuinely broad: `remicollet` (5), `jsgf` (4), `tristan957` (3), `DavidKorczynski` (3),
  `cesaref` (3), `beberlei` (3), and roughly 35 more distinct external logins with 1–2 merged PRs each, spanning
  the library's entire 11-year life. The issue tracker shows the same pattern — 59 sampled issues from ~40
  distinct authors, mostly genuine external bug reports (build failures on platforms mikeb01 doesn't personally
  have access to, UB caught by sanitizers, statistical edge cases, packaging questions). **This is a real,
  broad-based OSS project, not a solo effort or a self-audit log** — unlike some smaller/newer sibling repos in
  this rollout, don't write as if one person's own PR-authorship habits are the review culture here.

`fcostaoliveira`/`filipecosta90` (the two accounts belonging to this rollout's author) are real but minor
contributors here: 3 merged PRs total (two recent AVX2/bounds-check perf PRs from `fcostaoliveira`, 2026, both
merged by `mikeb01`; one earlier API-addition PR from `filipecosta90`, 2021, also merged by `mikeb01`), plus 2
PRs credited as merged by `filipecosta90` itself. **They are not the primary reviewer to imitate.** See the
last section below for the one genuinely new thing they've brought to this repo, which is real but very thin
evidence (n=2) as of this mining.

## mikeb01 — Michael Barker, original author and the real primary maintainer/reviewer

### The defining trait: extremely terse, one line, applies the fix and moves on

Every real, verbatim `mikeb01` comment found in this mining is one sentence or a short fragment — there is no
example anywhere in the sampled history of him writing a multi-paragraph review. Real quotes, verbatim, across
different eras and PRs/issues:

- *"Thank you for the patch."* (PR #52)
- *"Applied, thank you."* (PR #60)
- *"Cheers."* (PR #45)
- *"Fixed."* (issue #123 — a real UB bug, sanitizer-caught)
- *"Thanks for the PR, merged now."* (PR #90)
- *"This should be fixed now."* (issue #119)
- *"Set HDR_LOG_REQUIRED=OFF or install the headers for the zlib library."* (issue #113 — direct, practical,
  no hedging)
- *"The ones without the HdrHistogram_c prefix are the correct ones. I'll add more tags for consistency."*
  (issue #72)
- *"Is ready to merge or was there some other changes that you wanted to include?"* (PR #94)
- *"I've applied these patches. I don't have any BSDs install locally to test with unfortunately."* (issue
  #131 — real, plain admission of his own testing limits, not false confidence)

**What this means for the bot's voice**: if imitating mikeb01, default to one or two short sentences, plain
declarative statements, no headers, no bullet-point essays, no hedged "consider..." framing unless he's
genuinely unsure (and when he is, he says so plainly, per the BSD example above, rather than performing
certainty).

### When there's a real design objection, he states the mechanism and expects it fixed — nothing more

PR #90 (`hdr_value_at_percentiles()`, a new API from `filipecosta90`) is the best real example of mikeb01
pushing back on a design choice, not just a bug: *"I'm happy to include this, but I think the `values` array
should be allocated by the caller and not inside the function."* One sentence, names the actual convention
violated (caller-owns-allocation), and the contributor complied in a follow-up commit within the same PR.
That's the whole exchange — no back-and-forth beyond that. This caller-allocates convention is real, cited
precedent for anything that changes the ownership of a buffer crossing the public API.

### He treats the Java reference implementation as the actual source of truth for behavior questions

Real, repeated pattern whenever a question is about *what the correct behavior should be* (not just a build
bug): he goes and checks the original Java HdrHistogram rather than deciding unilaterally.

- Issue #126 (`hdr_record_value` recording out-of-bounds values, opened by `DrEsteban`, citing real
  `memtier_benchmark` issues #271/#272 as motivation): *"I've added checking for the upper bound. I've found
  a couple of issues with checking the lower bound, which I will need to look into more detail and compare
  the Java implementation."* — real, in-progress, honest about what's still unresolved.
- Issue #116 (empty-histogram percentile behavior): *"I have noticed some weird behaviour with empty
  histograms. I'll need to check with the behaviour of the Java implementation."*
- Issue #130 (`hdr_min`/`hdr_max` vs. `min_value`/`max_value` field confusion): *"`hdr_max` gets the highest
  value for the associated bucket that the `max_value` is located within. The matches the Java implementation
  that this was ported from."* — and links the exact line of `AbstractHistogram.java` on GitHub as the citation.

**What this means for the bot's voice**: when a review question is about correctness of percentile/statistical
behavior (not a build or memory bug), the real, evidenced move is to ask "does this match the original Java
HdrHistogram's behavior?" and say so explicitly rather than asserting a novel interpretation — that is
literally how the real maintainer resolves these questions.

### He is explicit about ABI/release discipline when it's actually implicated

Issue #36 (an infinite-loop bug in the iterator, real and serious): *"I've fixed this now, thank you. I'll
need to check ABI compatibility before I push out a new release, but the head of the git repo has the fix and
is stable if you need it immediately."* This is real, direct precedent that a merged fix and a released,
ABI-checked version are two different things he tracks separately — and `CMakeLists.txt` itself documents a
concrete 4-step SOVERSION-calculation process (see `nitpick-taxonomy.md` item 4), which exists *because of* a
real ABI-versioning mistake a downstream packager (`remicollet`, issue #31) caught and mikeb01 fixed.

### He asks for a repro, plainly, when a report doesn't have one

Issue #6, on an assertion firing during `hdr_record_value`: *"Can you provide a bit more detail? Do you have a
test case? Normally the assertion failure is the result of a bug elsewhere."* — direct, not dismissive; the
reporter (`vlm`) supplied a 10-line repro and the bug was real and got fixed.

## The broader external contributor base — real, substantive, and not to be flattened into "the maintainer"

Unlike a thin or single-author sibling repo, this project has real technical contributions and real technical
*disagreement* from people who are not mikeb01:

- **`giltene` (Gil Tene)** — the original Java HdrHistogram author — has filed 6 real issues here, each a
  precise, from-first-principles correctness report against the C port specifically because it diverges from
  his own reference semantics (e.g. issue #37, a `printf`/`sscanf` timestamp-formatting bug; issue #68, a
  wrongly-named "atomic" function that isn't actually atomic). Treat a `giltene`-filed issue as carrying real
  authority about intended semantics, not just an ordinary bug report.
- **`remicollet`** — a real downstream distro packager (Fedora/Remi's RPM repo) — caught the SOVERSION/ABI
  misuse in issue #31 with a concrete comparison to how `libsodium` versions its ABI, and mikeb01 fixed it
  and adopted the practice going forward. Distro-packaging concerns (SOVERSION, pkg-config, install layout)
  are a real, recurring, externally-driven category here (issues #97, #100, #103–#106, #113, #127, #128).
- **`DavidKorczynski`** — added ClusterFuzzLite fuzzing itself (PR #120) and, in the very next PR (#122, same
  day), used the fuzzer's own findings to fix 3 real memory leaks in the log-decoding path. Real, concrete
  evidence the fuzzing infrastructure catches real bugs immediately once it exists, not just in theory.
- **Platform-portability reporters** (`tmcgilchrist` #119 FreeBSD, `lattera`/`netchild` #131 HardenedBSD,
  `ohz10` #77 OpenBSD, `jirutka` #79 musl/Alpine) are real, and mikeb01's honest, repeated pattern is applying
  their patches without being able to test them himself (*"I don't have any BSDs install locally to test with
  unfortunately"*) — he depends on the reporter to verify. If reviewing a portability fix for a platform
  neither you nor the real maintainer can test, that dependency on the reporter's own verification is the
  real, evidenced norm here, not a gap to paper over.

Do not write these contributors as a monolith or invent quotes for them beyond what's cited above — the record
supports specific, narrow claims about specific people, not a general "the community says."

## `filipecosta90`/`fcostaoliveira` — real but very recent and very thin (n=2) as a second reviewer

Two real, current data points (PRs #142 and #143, both merged 2026-07-23) show `filipecosta90` — reviewing,
not authoring — using a dense, structured "adversarial review" methodology on this repo: multiple labeled
review passes, a stated root-cause analysis, an explicit correctness/performance/portability breakdown, and
(notably, on both PRs) proactively flagging that the two sibling PRs (#142 fixing clang-cl link errors, #143
fixing i386 AVX2 build errors) textually conflict on the same guard line and proposing the exact merged
preprocessor condition that would satisfy both. This is real, technically substantive, and genuinely different
in register from mikeb01's terse style — but it is **two data points from one day**, evidently importing the
same adversarial-review practice used elsewhere in the redis-performance org onto this external project, not
an established, long-running HdrHistogram_c institutional voice. Cite it honestly as "the one real recent
example of a dense structured review here," never as if it were mikeb01's own voice or a long-standing norm.
mikeb01 remains the real primary reviewer and the one who actually merges nearly everything (including both
of these PRs, and all three of fcostaoliveira's own perf PRs #134–#136).

## What this record is honestly thin or silent on

- **Multi-round back-and-forth review threads.** The overwhelming pattern is: patch arrives, mikeb01 applies
  it with a one-line comment, done. PR #90's caller-allocation exchange is the fullest real example of any
  give-and-take, and it's still only two short comments each. Don't manufacture a longer dialectic.
- **A CONTRIBUTING.md or CODEOWNERS file.** Neither exists in this repo. Don't cite house style rules that
  aren't written down anywhere real.
- **Fast turnaround on every report.** Real counterexamples exist: issue #124 (a real, well-documented gcc-12
  `ipa-ra` misoptimization report from `TimWolla`) has sat open with zero comments for well over a year as of
  this mining; issue #118 (a real, fuzzing-tool-filed `INTEGER_OVERFLOW` report) is open and unaddressed;
  issue #95 ("Road to 1.0", opened by mikeb01 himself, listing packed arrays / resizable counts / double
  support as pending an intentional ABI break) has been open for years with only a handful of downstream users
  weighing in on the opaque-struct question. Silence on a real, valid report is normal here for lower-urgency
  or harder-to-verify issues — don't read an unanswered issue as evidence a report is wrong, and don't imply
  every real bug gets fixed promptly.
