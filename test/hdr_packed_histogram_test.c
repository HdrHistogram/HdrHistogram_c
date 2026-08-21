/*
 * packed_test.c -- behavioral parity + coverage suite for the packed variant.
 *
 * Java HdrHistogram has no standalone "packed" tests: PackedHistogram is tested
 * purely as a drop-in parametrized variant of the normal histogram suite. So the
 * ports below take each applicable Java @Test/@ParameterizedTest that includes
 * PackedHistogram and run it as a DENSE-vs-PACKED parity check with identical
 * assertions -- the exact Java pattern ("same asserts, different class"). Each
 * test notes the Java method it ports. Tests whose Java counterpart exercises a
 * feature this Phase-2 core does not implement (add/subtract/copy/shift/
 * iterators/auto-resize) are listed in PORT-MATRIX.md, not here.
 *
 * minunit style, matching HdrHistogram_c/test.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_packed_histogram.h>
#include "minunit.h"

/* exported from the hdr static lib (declared in src/hdr_tests.h, not shipped) */
extern int hdr_encode_compressed(struct hdr_histogram*, uint8_t**, size_t*);
extern int hdr_decode_compressed(uint8_t*, size_t, struct hdr_histogram**);

int tests_run = 0;
/* compare_double / compare_int64 come from minunit.c (declared in minunit.h). */

#define HIGHEST 3600000000LL
#define SIG 3

static uint64_t rng = 0x123456789abcdefULL;
static uint64_t xr(void) { uint64_t x = rng; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; rng = x; return x * 0x2545F4914F6CDD1DULL; }

/* ---- parity helper: same records into dense+packed, assert identical ------ */
static char* parity_full(struct hdr_histogram* d, struct hdr_packed_histogram* p)
{
    mu_assert("total mismatch", compare_int64(d->total_count, hdr_packed_total_count(p)));
    mu_assert("min mismatch", compare_int64(hdr_min(d), hdr_packed_min(p)));
    mu_assert("max mismatch", compare_int64(hdr_max(d), hdr_packed_max(p)));
    /* index-by-index count parity (ports HistogramTest.testPackedEquivalence) */
    for (int32_t i = 0; i < d->counts_len; i++)
    {
        int64_t cd = hdr_count_at_index(d, i);
        int64_t v = hdr_value_at_index(d, i);
        mu_assert("count-at-index parity", compare_int64(cd, hdr_packed_count_at_value(p, v)));
    }
    /* percentile parity */
    const double pcts[] = {0, 1, 10, 25, 50, 75, 90, 99, 99.9, 100};
    for (size_t i = 0; i < sizeof(pcts)/sizeof(pcts[0]); i++)
        mu_assert("percentile parity",
            compare_int64(hdr_value_at_percentile(d, pcts[i]), hdr_packed_value_at_percentile(p, pcts[i])));
    /* mean/stddev parity (non-empty): tolerance from Java data-access tests */
    if (d->total_count > 0)
    {
        mu_assert("mean parity", compare_double(hdr_mean(d), hdr_packed_mean(p), hdr_mean(d) * 0.001 + 1e-9));
        mu_assert("stddev parity", compare_double(hdr_stddev(d), hdr_packed_stddev(p), hdr_stddev(d) * 0.001 + 1e-9));
    }
    return 0;
}

/* ports HistogramTest.testConstructionArgumentRanges */
static char* test_construction_argument_ranges(void)
{
    struct hdr_packed_histogram* p = (void*)0x1;
    mu_assert("low<1 -> EINVAL", hdr_packed_init(0, HIGHEST, SIG, &p) == EINVAL);
    mu_assert("sig<1 -> EINVAL", hdr_packed_init(1, HIGHEST, 0, &p) == EINVAL);
    mu_assert("sig>5 -> EINVAL", hdr_packed_init(1, HIGHEST, 6, &p) == EINVAL);
    mu_assert("low*2>high -> EINVAL", hdr_packed_init(100, 100, SIG, &p) == EINVAL);
    return 0;
}

/* ports HistogramTest.testEmptyHistogram */
static char* test_empty_histogram(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    mu_assert("empty total", hdr_packed_total_count(p) == 0);
    mu_assert("empty max 0", hdr_packed_max(p) == 0);
    mu_assert("empty mean 0", hdr_packed_mean(p) == 0.0);
    mu_assert("empty stddev 0", hdr_packed_stddev(p) == 0.0);
    mu_assert("empty populated 0", hdr_packed_populated(p) == 0);
    mu_assert("empty min == dense", hdr_packed_min(p) == hdr_min(d));
    char* r = parity_full(d, p);
    hdr_packed_close(p); hdr_close(d);
    return r;
}

/* ports HistogramTest.testRecordValue */
static char* test_record_value(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    hdr_packed_record_value(p, 4); hdr_record_value(d, 4);
    mu_assert("count at 4", hdr_packed_count_at_value(p, 4) == 1);
    mu_assert("total 1", hdr_packed_total_count(p) == 1);
    char* r = parity_full(d, p);
    hdr_packed_close(p); hdr_close(d);
    return r;
}

/* ports HistogramTest.testRecordValue_Overflow (returns false vs Java throw) */
static char* test_record_value_overflow(void)
{
    struct hdr_packed_histogram* p = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    mu_assert("over-range -> false", !hdr_packed_record_value(p, HIGHEST * 3));
    mu_assert("negative -> false", !hdr_packed_record_value(p, -1));
    mu_assert("still empty", hdr_packed_total_count(p) == 0);
    mu_assert("count_at_value out-of-range 0", hdr_packed_count_at_value(p, HIGHEST * 3) == 0);
    hdr_packed_close(p);
    return 0;
}

/* ports HistogramTest.testReset */
static char* test_reset(void)
{
    struct hdr_packed_histogram* p = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_packed_record_value(p, 4); hdr_packed_record_value(p, 10); hdr_packed_record_value(p, 100);
    hdr_packed_reset(p);
    mu_assert("reset total 0", hdr_packed_total_count(p) == 0);
    mu_assert("reset populated 0", hdr_packed_populated(p) == 0);
    mu_assert("reset width 1", hdr_packed_count_width(p) == 1);
    mu_assert("reset count 0", hdr_packed_count_at_value(p, 4) == 0);
    hdr_packed_record_value(p, 20); hdr_packed_record_value(p, 80);
    /* compare post-reset min against a fresh DENSE oracle (not self) */
    struct hdr_histogram* d = NULL;
    hdr_init(1, HIGHEST, SIG, &d);
    hdr_record_value(d, 20); hdr_record_value(d, 80);
    mu_assert("post-reset min parity", compare_int64(hdr_min(d), hdr_packed_min(p)));
    mu_assert("post-reset total 2", hdr_packed_total_count(p) == 2);
    hdr_close(d); hdr_packed_close(p);
    return 0;
}

/* count-width boundary grid ±1 with dense parity (ports the Java 2^N grid; adds
   the 2^53..2^62 range no Float64-based port can test) */
static char* test_count_width_boundaries(void)
{
    struct { int64_t c; int w; } cases[] = {
        {255, 1}, {256, 2}, {65535, 2}, {65536, 4},
        {4294967295LL, 4}, {4294967296LL, 8},
        {1LL << 53, 8}, {1LL << 62, 8},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
    {
        struct hdr_packed_histogram* p = NULL;
        struct hdr_histogram* d = NULL;
        hdr_packed_init(1, HIGHEST, SIG, &p);
        hdr_init(1, HIGHEST, SIG, &d);
        hdr_packed_record_values(p, 6147, cases[i].c);
        hdr_record_values(d, 6147, cases[i].c);
        mu_assert("boundary width", hdr_packed_count_width(p) == cases[i].w);
        mu_assert("boundary count parity", compare_int64(hdr_count_at_value(d, 6147), hdr_packed_count_at_value(p, 6147)));
        mu_assert("boundary count exact", hdr_packed_count_at_value(p, 6147) == cases[i].c);
        mu_assert("boundary total parity", compare_int64(d->total_count, hdr_packed_total_count(p)));
        /* encode round-trip preserves the wide count */
        uint8_t* sp = NULL; size_t spl = 0;
        mu_assert("boundary encode", hdr_packed_encode_compressed(p, &sp, &spl) == 0);
        struct hdr_histogram* dfp = NULL;
        mu_assert("boundary decode", hdr_decode_compressed(sp, spl, &dfp) == 0);
        mu_assert("boundary rt count", compare_int64(cases[i].c, hdr_count_at_value(dfp, 6147)));
        free(sp); hdr_close(dfp);
        hdr_packed_close(p); hdr_close(d);
    }
    return 0;
}

/* re-pack survival: many distinct buckets, then force 1->2->4->8 widening;
   every prior count must survive each re-pack (JS "copy data when resizing") */
static char* test_repack_survival(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    rng = 0x5151;
    /* populate ~400 distinct buckets at width 1 */
    for (int i = 0; i < 400; i++) { int64_t v = (int64_t)(xr() % (uint64_t)HIGHEST) + 1; hdr_packed_record_value(p, v); hdr_record_value(d, v); }
    /* now drive one bucket up through every width, re-checking ALL buckets survive */
    int64_t bumps[] = {300, 70000, 5000000000LL};
    for (int b = 0; b < 3; b++)
    {
        hdr_packed_record_values(p, 123456789, bumps[b]);
        hdr_record_values(d, 123456789, bumps[b]);
        for (int32_t idx = 0; idx < d->counts_len; idx++)
            mu_assert("repack survival", compare_int64(hdr_count_at_index(d, idx), hdr_packed_count_at_value(p, hdr_value_at_index(d, idx))));
    }
    mu_assert("repack width 8", hdr_packed_count_width(p) == 8);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* count==0 parity with dense; count<0 rejected; value boundary at highest */
static char* test_count_and_value_edges(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    /* count 0: dense updates min/max, total unchanged; packed must match */
    mu_assert("count0 returns true", hdr_packed_record_values(p, 500, 0));
    hdr_record_values(d, 500, 0);
    mu_assert("count0 total parity", compare_int64(d->total_count, hdr_packed_total_count(p)));
    mu_assert("count0 max parity", compare_int64(hdr_max(d), hdr_packed_max(p)));
    /* count < 0 rejected */
    mu_assert("neg count rejected", !hdr_packed_record_values(p, 500, -1));
    /* value boundary: highest ok, highest+1 rejected */
    mu_assert("highest recorded", hdr_packed_record_value(p, HIGHEST));
    mu_assert("highest+1 rejected", !hdr_packed_record_value(p, HIGHEST + 1));
    /* count overflow on an existing bucket (INT64_MAX + 1) is refused, count kept */
    struct hdr_packed_histogram* q = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &q);
    mu_assert("record INT64_MAX", hdr_packed_record_values(q, 500, INT64_MAX));
    mu_assert("overflow-on-hit refused", !hdr_packed_record_values(q, 500, 1));
    mu_assert("count kept at INT64_MAX", hdr_packed_count_at_value(q, 500) == INT64_MAX);
    /* total_count==INT64_MAX makes the percentile double product reach 2^63,
       firing the cast clamp; must not crash and must resolve to bucket 500. */
    mu_assert("p100 at INT64_MAX total", hdr_packed_value_at_percentile(q, 100.0) == hdr_packed_max(q));
    hdr_packed_close(q);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* empty-histogram encode round-trips to an empty dense histogram (covers the
   trailing zero-run path and the total_count==0 encode) */
static char* test_encode_empty(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    uint8_t *sp = NULL, *sd = NULL; size_t spl = 0, sdl = 0;
    mu_assert("empty packed encode", hdr_packed_encode_compressed(p, &sp, &spl) == 0);
    mu_assert("empty dense encode", hdr_encode_compressed(d, &sd, &sdl) == 0);
    mu_assert("empty byte-identical", spl == sdl && memcmp(sp, sd, spl) == 0);
    struct hdr_histogram* dfp = NULL;
    mu_assert("empty decode", hdr_decode_compressed(sp, spl, &dfp) == 0);
    mu_assert("empty decoded total 0", dfp->total_count == 0);
    free(sp); free(sd); hdr_close(dfp);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* percentile query robustness: NaN / +Inf / -Inf / negative must not crash and
   should match dense (ports Go FuzzPercentileQueries) */
static char* test_percentile_robustness(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    for (int i = 1; i <= 50; i++) { hdr_packed_record_value(p, i * 7000); hdr_record_value(d, i * 7000); }
    /* Adversarial percentiles are a packed-robustness property, not a dense-parity
       one: dense casts (p/100)*total+0.5 to int64 with no guard, so -Inf here is a
       float-cast-overflow (UB) in the dense query path -- packed guards it. Assert
       packed stays in range and does not crash; do not invoke the dense UB. */
    double weird[] = { NAN, INFINITY, -INFINITY, -5.0, 1e300 };
    int64_t wmax = hdr_packed_max(p);
    for (int i = 0; i < 5; i++)
    {
        int64_t v = hdr_packed_value_at_percentile(p, weird[i]);
        mu_assert("weird percentile stays in range", v >= 0 && v <= wmax);
    }
    /* also on an empty histogram (no crash) */
    struct hdr_packed_histogram* e = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &e);
    (void) hdr_packed_value_at_percentile(e, NAN);
    (void) hdr_packed_value_at_percentile(e, 50.0);
    hdr_packed_close(e);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* encode after populate->reset->populate round-trips correctly */
static char* test_encode_after_reset(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    for (int i = 0; i < 100; i++) hdr_packed_record_values(p, i * 1000 + 1, 70000); /* widen to 4B */
    hdr_packed_reset(p);
    /* reset mirrors dense: retains width+capacity (reclaimed only at close), so
       width stays 4 and re-recording reuses the wider slots correctly. */
    mu_assert("reset retains width", hdr_packed_count_width(p) == 4);
    mu_assert("reset empties", hdr_packed_total_count(p) == 0 && hdr_packed_populated(p) == 0);
    for (int i = 0; i < 30; i++) { hdr_packed_record_value(p, i * 5000 + 1); hdr_record_value(d, i * 5000 + 1); }
    uint8_t *sp = NULL, *sd = NULL; size_t spl = 0, sdl = 0;
    mu_assert("post-reset encode", hdr_packed_encode_compressed(p, &sp, &spl) == 0);
    mu_assert("dense encode", hdr_encode_compressed(d, &sd, &sdl) == 0);
    mu_assert("post-reset byte-identical", spl == sdl && memcmp(sp, sd, spl) == 0);
    free(sp); free(sd);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* ports HistogramTest.testValueAtPercentileMatchesPercentile */
static char* test_value_at_percentile_matches_percentile(void)
{
    const int lengths[] = {1, 5, 10, 50, 100, 500, 1000, 5000, 10000};
    for (size_t li = 0; li < sizeof(lengths)/sizeof(lengths[0]); li++)
    {
        int length = lengths[li];
        struct hdr_packed_histogram* p = NULL;
        struct hdr_histogram* d = NULL;
        hdr_packed_init(1, INT64_MAX, 2, &p);
        hdr_init(1, INT64_MAX, 2, &d);
        for (int v = 1; v <= length; v++) { hdr_packed_record_value(p, v); hdr_record_value(d, v); }
        for (int v = 1; v <= length; v++)
        {
            double pct = (100.0 * v) / length;
            mu_assert("percentile-matches parity",
                compare_int64(hdr_value_at_percentile(d, pct), hdr_packed_value_at_percentile(p, pct)));
        }
        hdr_packed_close(p); hdr_close(d);
    }
    return 0;
}

/* ports HistogramTest.testPackedEquivalence + general random parity */
static char* test_packed_equivalence_random(void)
{
    rng = 0xEE;
    for (int trial = 0; trial < 5; trial++)
    {
        struct hdr_packed_histogram* p = NULL;
        struct hdr_histogram* d = NULL;
        hdr_packed_init(1, HIGHEST, SIG, &p);
        hdr_init(1, HIGHEST, SIG, &d);
        int n = 1000 + trial * 3000;
        for (int i = 0; i < n; i++)
        {
            int64_t v = (int64_t)(xr() % (uint64_t)HIGHEST) + 1;
            int64_t c = 1 + (int64_t)(xr() % 9);
            hdr_packed_record_values(p, v, c);
            hdr_record_values(d, v, c);
        }
        char* r = parity_full(d, p);
        hdr_packed_close(p); hdr_close(d);
        if (r) return r;
    }
    return 0;
}

/* ports HistogramTest.testConstructionWithLargeNumbers (value-equivalent percentiles) */
static char* test_construction_with_large_numbers(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(20000000, 100000000, 5, &p);
    hdr_init(20000000, 100000000, 5, &d);
    int64_t vs[] = {100000000, 20000000, 30000000};
    for (int i = 0; i < 3; i++) { hdr_packed_record_value(p, vs[i]); hdr_record_value(d, vs[i]); }
    double pcts[] = {50.0, 83.33, 83.34, 99.0};
    for (int i = 0; i < 4; i++)
        mu_assert("large-numbers percentile parity",
            compare_int64(hdr_value_at_percentile(d, pcts[i]), hdr_packed_value_at_percentile(p, pcts[i])));
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* ports HistogramEncodingTest.testHistogramEncoding (V2 round-trip, both ways) */
static char* test_histogram_encoding(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    for (int i = 0; i < 10000; i++) { hdr_packed_record_value(p, 3000LL * i + 1); hdr_record_value(d, 3000LL * i + 1); }

    /* packed encode -> dense decode == dense (full counts) */
    uint8_t* sp = NULL; size_t spl = 0;
    mu_assert("packed encode ok", hdr_packed_encode_compressed(p, &sp, &spl) == 0);
    struct hdr_histogram* dfp = NULL;
    mu_assert("dense decode ok", hdr_decode_compressed(sp, spl, &dfp) == 0);
    mu_assert("decoded total", compare_int64(d->total_count, dfp->total_count));
    for (int32_t i = 0; i < d->counts_len; i++)
        mu_assert("decoded counts parity", compare_int64(hdr_count_at_index(d, i), hdr_count_at_index(dfp, i)));

    /* dense encode -> packed decode == dense (queries) */
    uint8_t* sd = NULL; size_t sdl = 0;
    mu_assert("dense encode ok", hdr_encode_compressed(d, &sd, &sdl) == 0);
    struct hdr_packed_histogram* prt = NULL;
    mu_assert("packed decode ok", hdr_packed_decode_compressed(sd, sdl, &prt) == 0);
    char* r = parity_full(d, prt);

    /* byte-identical stream */
    mu_assert("byte-identical stream", spl == sdl && memcmp(sp, sd, spl) == 0);

    free(sp); free(sd); hdr_close(dfp); hdr_packed_close(prt);
    hdr_packed_close(p); hdr_close(d);
    return r;
}

/* ports HistogramEncodingTest.testSimpleIntegerHistogramEncoding (count-width growth) */
static char* test_encoding_count_width_growth(void)
{
    /* push a single bucket's count across 2^8, 2^16, 2^32, 2^52 and round-trip */
    int64_t steps[] = {200, 70000, 5000000000LL, 5000000000000000LL};
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    for (int s = 0; s < 4; s++)
    {
        hdr_packed_record_values(p, 6147, steps[s]);
        hdr_record_values(d, 6147, steps[s]);
        uint8_t* sp = NULL; size_t spl = 0;
        mu_assert("wgrowth encode", hdr_packed_encode_compressed(p, &sp, &spl) == 0);
        struct hdr_histogram* dfp = NULL;
        mu_assert("wgrowth decode", hdr_decode_compressed(sp, spl, &dfp) == 0);
        mu_assert("wgrowth total parity", compare_int64(d->total_count, dfp->total_count));
        mu_assert("wgrowth count parity", compare_int64(hdr_count_at_value(d, 6147), hdr_count_at_value(dfp, 6147)));
        free(sp); hdr_close(dfp);
    }
    mu_assert("final width 8", hdr_packed_count_width(p) == 8);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* our added code: count-width lands at each width; slot_get/set at 1/2/4/8 */
static char* test_count_widths(void)
{
    struct hdr_packed_config* cfg = NULL;
    hdr_packed_config_create(1, HIGHEST, SIG, &cfg);
    struct { int64_t count; int expect_width; } cases[] = {
        {1, 1}, {200, 1}, {300, 2}, {70000, 4}, {5000000000LL, 8}
    };
    for (int i = 0; i < 5; i++)
    {
        struct hdr_packed_histogram* p = NULL;
        hdr_packed_init_shared(cfg, &p);
        hdr_packed_record_values(p, 12345, cases[i].count);
        mu_assert("width as expected", hdr_packed_count_width(p) == cases[i].expect_width);
        mu_assert("count round-trips at width", hdr_packed_count_at_value(p, 12345) == cases[i].count);
        hdr_packed_close(p);
    }
    hdr_packed_config_destroy(cfg);
    return 0;
}

/* our added code: percentile edge branches (==0, >=100), value_at_percentiles */
static char* test_percentile_edges(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    for (int i = 1; i <= 100; i++) { hdr_packed_record_value(p, i * 1000); hdr_record_value(d, i * 1000); }
    mu_assert("p0 parity", compare_int64(hdr_value_at_percentile(d, 0.0), hdr_packed_value_at_percentile(p, 0.0)));
    mu_assert("p100 parity", compare_int64(hdr_value_at_percentile(d, 100.0), hdr_packed_value_at_percentile(p, 100.0)));
    mu_assert("p>100 clamp parity", compare_int64(hdr_value_at_percentile(d, 100.0), hdr_packed_value_at_percentile(p, 250.0)));

    double pcts[] = {0, 50, 90, 99, 100};
    int64_t pv[5], dv[5];
    mu_assert("value_at_percentiles ok", hdr_packed_value_at_percentiles(p, pcts, pv, 5) == 0);
    hdr_value_at_percentiles(d, pcts, dv, 5);
    for (int i = 0; i < 5; i++) mu_assert("plural parity", compare_int64(dv[i], pv[i]));
    mu_assert("plural null percentiles EINVAL", hdr_packed_value_at_percentiles(p, NULL, pv, 5) == EINVAL);
    mu_assert("plural null values EINVAL", hdr_packed_value_at_percentiles(p, pcts, NULL, 5) == EINVAL);
    /* divergence #8: packed plural == packed singular at p0 (bucket bottom),
       self-consistent (dense's plural returns the bucket top -- not reproduced). */
    double zero[1] = {0.0}; int64_t zpl[1];
    hdr_packed_value_at_percentiles(p, zero, zpl, 1);
    mu_assert("plural p0 == singular p0", zpl[0] == hdr_packed_value_at_percentile(p, 0.0));
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* our added code: min with bucket-0 populated (record 0) */
static char* test_min_with_zero(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_init(1, HIGHEST, SIG, &d);
    hdr_packed_record_value(p, 0); hdr_record_value(d, 0);
    hdr_packed_record_value(p, 100); hdr_record_value(d, 100);
    mu_assert("min zero parity", compare_int64(hdr_min(d), hdr_packed_min(p)));
    mu_assert("count at 0 parity", compare_int64(hdr_count_at_value(d, 0), hdr_packed_count_at_value(p, 0)));
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* our added code: memory-size accounting (owned vs shared config) + populated */
static char* test_memory_size(void)
{
    struct hdr_packed_histogram* owned = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &owned);
    size_t m0 = hdr_packed_get_memory_size(owned);
    for (int i = 0; i < 50; i++) hdr_packed_record_value(owned, i * 1000 + 1);
    size_t m1 = hdr_packed_get_memory_size(owned);
    mu_assert("owned mem grows", m1 > m0);
    mu_assert("owned populated", hdr_packed_populated(owned) > 0);

    struct hdr_packed_config* cfg = NULL;
    hdr_packed_config_create(1, HIGHEST, SIG, &cfg);
    mu_assert("config mem > 0", hdr_packed_config_memory_size(cfg) > 0);
    struct hdr_packed_histogram* shared = NULL;
    hdr_packed_init_shared(cfg, &shared);
    for (int i = 0; i < 50; i++) hdr_packed_record_value(shared, i * 1000 + 1);
    /* shared excludes config; owned includes it -> owned larger at equal contents */
    mu_assert("owned includes config, shared excludes",
        hdr_packed_get_memory_size(owned) > hdr_packed_get_memory_size(shared));
    hdr_packed_close(owned); hdr_packed_close(shared); hdr_packed_config_destroy(cfg);
    return 0;
}

/* decode real shapes through the PACKED decoder (not just dense): wide-count
   (decode-time widen), empty, and zero-only (recompute max_i/min_nz branches) */
static char* test_packed_decode_shapes(void)
{
    /* wide count -> packed decoder must widen during decode */
    struct hdr_packed_histogram* p = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    hdr_packed_record_values(p, 6147, 5000000000LL);
    hdr_packed_record_value(p, 100);
    uint8_t* s = NULL; size_t sl = 0;
    mu_assert("wide encode", hdr_packed_encode_compressed(p, &s, &sl) == 0);
    struct hdr_packed_histogram* out = NULL;
    mu_assert("packed decode wide ok", hdr_packed_decode_compressed(s, sl, &out) == 0);
    mu_assert("packed decode wide count", hdr_packed_count_at_value(out, 6147) == 5000000000LL);
    mu_assert("packed decode wide width", hdr_packed_count_width(out) == 8);
    mu_assert("packed decode wide total", hdr_packed_total_count(out) == 5000000001LL);
    free(s); hdr_packed_close(out); hdr_packed_close(p);

    /* empty stream -> recompute max_i==-1 && min_nz==-1 */
    struct hdr_packed_histogram* e = NULL; hdr_packed_init(1, HIGHEST, SIG, &e);
    uint8_t* se = NULL; size_t sel = 0;
    hdr_packed_encode_compressed(e, &se, &sel);
    struct hdr_packed_histogram* eo = NULL;
    mu_assert("packed decode empty ok", hdr_packed_decode_compressed(se, sel, &eo) == 0);
    mu_assert("packed decode empty total", hdr_packed_total_count(eo) == 0);
    mu_assert("packed decode empty min", hdr_packed_min(eo) == INT64_MAX);
    mu_assert("packed decode empty max", hdr_packed_max(eo) == 0);
    free(se); hdr_packed_close(eo); hdr_packed_close(e);

    /* only bucket 0 populated -> recompute min_nz==-1 but max_i==0 */
    struct hdr_packed_histogram* z = NULL; hdr_packed_init(1, HIGHEST, SIG, &z);
    hdr_packed_record_value(z, 0);
    uint8_t* sz = NULL; size_t szl = 0;
    hdr_packed_encode_compressed(z, &sz, &szl);
    struct hdr_packed_histogram* zo = NULL;
    mu_assert("packed decode zero-only ok", hdr_packed_decode_compressed(sz, szl, &zo) == 0);
    mu_assert("packed decode zero-only min", hdr_packed_min(zo) == 0);
    mu_assert("packed decode zero-only total", hdr_packed_total_count(zo) == 1);
    free(sz); hdr_packed_close(zo); hdr_packed_close(z);
    return 0;
}

/* top bucket at highest_trackable_value == INT64_MAX: the bucket's upper edge
   exceeds INT64_MAX, so the highest-equivalent computation must not overflow
   (UB) -- on max(), value_at_percentile(), and the decode recompute path. */
static char* test_top_bucket_int64_max(void)
{
    struct hdr_packed_histogram* p = NULL;
    struct hdr_histogram* d = NULL;
    hdr_packed_init(1, INT64_MAX, 3, &p);
    hdr_init(1, INT64_MAX, 3, &d);
    hdr_packed_record_value(p, INT64_MAX);
    hdr_record_value(d, INT64_MAX);
    /* Packed saturates the top bucket's highest-equivalent to INT64_MAX instead of
       overflowing int64. The dense query path (hdr_max / value_at_percentile) does
       overflow here (UB) on unpatched trees, so assert packed's documented behavior
       directly rather than comparing to a UB dense call. */
    int64_t pmax = hdr_packed_max(p);
    mu_assert("top-bucket max saturates to INT64_MAX", pmax == INT64_MAX);
    mu_assert("top-bucket p100 == max", hdr_packed_value_at_percentile(p, 100.0) == pmax);
    mu_assert("top-bucket p50 == max (single bucket)", hdr_packed_value_at_percentile(p, 50.0) == pmax);
    /* decode recompute path: dense-encode a top-bucket stream (encode serializes the
       stored min/max, no highest-equivalent -> safe), decode via packed, check packed
       self-consistency. */
    uint8_t* sd = NULL; size_t sdl = 0;
    mu_assert("dense encode top", hdr_encode_compressed(d, &sd, &sdl) == 0);
    struct hdr_packed_histogram* prt = NULL;
    mu_assert("packed decode top", hdr_packed_decode_compressed(sd, sdl, &prt) == 0);
    mu_assert("decoded top max parity", hdr_packed_max(prt) == pmax);
    mu_assert("decoded top total", hdr_packed_total_count(prt) == 1);
    free(sd); hdr_packed_close(prt);
    hdr_packed_close(p); hdr_close(d);
    return 0;
}

/* p100 (and NaN/+inf) must resolve to the LAST populated bucket even when
   total_count is in the band [~INT64_MAX-1022, INT64_MAX-1] where (double)total
   rounds up to 2^63 -- a clamp-to-INT64_MAX would make the target unreachable
   and wrongly return bucket 0. (Here packed is correct where dense is UB-wrong;
   compare to packed's own max, not dense -- documented divergence #5.) */
static char* test_p100_near_int64_max(void)
{
    struct hdr_packed_histogram* p = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p);
    /* single bucket, count in the rounding band */
    hdr_packed_record_values(p, 1000000, INT64_MAX - 1);
    int64_t mx = hdr_packed_max(p);
    mu_assert("band p100 == max", hdr_packed_value_at_percentile(p, 100.0) == mx);
    mu_assert("band pNaN == max", hdr_packed_value_at_percentile(p, NAN) == mx);
    mu_assert("band p+inf == max", hdr_packed_value_at_percentile(p, INFINITY) == mx);
    mu_assert("band p50 not top", hdr_packed_value_at_percentile(p, 50.0) == mx); /* single bucket */
    /* two buckets, total just below INT64_MAX (dense-UB band): p100 = LAST bucket */
    hdr_packed_reset(p);
    hdr_packed_record_values(p, 100, INT64_MAX / 2);
    hdr_packed_record_values(p, 1000000, INT64_MAX / 2 - 1);
    int64_t mx2 = hdr_packed_max(p);
    mu_assert("2-bucket band p100 == max (last bucket)",
        hdr_packed_value_at_percentile(p, 100.0) == mx2);
    hdr_packed_close(p);

    /* total > 2^53 (not the UB band): the p100 == max INVARIANT must hold -- packed
       returns the last/max bucket. dense's FP-rounded int64 target undershoots and
       returns an earlier bucket, so here packed is MORE correct than dense (a
       documented divergence, like mean/stddev in item 3). Assert the invariant, and
       that dense indeed differs (documenting the dense imprecision). */
    struct hdr_packed_histogram* p2 = NULL;
    struct hdr_histogram* d2 = NULL;
    hdr_packed_init(1, HIGHEST, SIG, &p2);
    hdr_init(1, HIGHEST, SIG, &d2);
    hdr_packed_record_values(p2, 1000, (1LL << 53));
    hdr_record_values(d2, 1000, (1LL << 53));
    hdr_packed_record_value(p2, HIGHEST);
    hdr_record_value(d2, HIGHEST);
    mu_assert("2^53 p100 == max (invariant)",
        hdr_packed_value_at_percentile(p2, 100.0) == hdr_packed_max(p2));
    mu_assert("2^53 dense p100 undershoots (packed more correct)",
        hdr_value_at_percentile(d2, 100.0) != hdr_packed_value_at_percentile(p2, 100.0));
    hdr_packed_close(p2); hdr_close(d2);
    return 0;
}

static struct mu_result all_tests(void)
{
    mu_run_test(test_construction_argument_ranges);
    mu_run_test(test_empty_histogram);
    mu_run_test(test_record_value);
    mu_run_test(test_record_value_overflow);
    mu_run_test(test_reset);
    mu_run_test(test_value_at_percentile_matches_percentile);
    mu_run_test(test_packed_equivalence_random);
    mu_run_test(test_construction_with_large_numbers);
    mu_run_test(test_histogram_encoding);
    mu_run_test(test_encoding_count_width_growth);
    mu_run_test(test_count_widths);
    mu_run_test(test_percentile_edges);
    mu_run_test(test_min_with_zero);
    mu_run_test(test_memory_size);
    mu_run_test(test_count_width_boundaries);
    mu_run_test(test_repack_survival);
    mu_run_test(test_count_and_value_edges);
    mu_run_test(test_encode_empty);
    mu_run_test(test_percentile_robustness);
    mu_run_test(test_encode_after_reset);
    mu_run_test(test_packed_decode_shapes);
    mu_run_test(test_top_bucket_int64_max);
    mu_run_test(test_p100_near_int64_max);
    mu_ok;
}

int main(void)
{
    struct mu_result result = all_tests();
    if (result.message != 0)
        printf("packed_test.%s(): %s\n", result.test, result.message);
    else
        printf("ALL TESTS PASSED\n");
    printf("Tests run: %d\n", tests_run);
    return result.message == NULL ? 0 : -1;
}
