/*
 * Structured fuzzer for the record / query / iterate / encode-decode / interval-
 * recorder surface of HdrHistogram_c. Derives construction parameters from the
 * input, then replays the remaining bytes as a stream of operations. Exercises
 * the hot read path (hdr_value_at_percentile, incl. the runtime AVX2 scan), the
 * iterators, histogram merge, the gzip+base64 encode/decode round-trip, and the
 * writer/reader-phaser interval recorder.
 *
 * Released to the public domain.
 */
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_histogram_log.h>
#include <hdr/hdr_interval_recorder.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Bounds chosen so a single input allocates a modest counts[] (keeps the
 * fuzzer fast: high exec/s) while still spanning enough sub-buckets that the
 * vectorized percentile scan runs many iterations. */
#define MAX_HIGHEST      ((int64_t)1 << 30)
#define MAX_OPS          2048
#define MAX_ITER_STEPS   10000

typedef struct { const uint8_t* p; size_t n; size_t i; } cursor;

static uint8_t u8(cursor* c)  { return c->i < c->n ? c->p[c->i++] : 0; }
static uint64_t u64(cursor* c)
{
    uint64_t v = 0;
    int k;
    for (k = 0; k < 8; k++) v = (v << 8) | u8(c);
    return v;
}

static void drain_iter(struct hdr_iter* iter)
{
    unsigned steps = 0;
    while (hdr_iter_next(iter) && ++steps < MAX_ITER_STEPS) { /* touch fields */ }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    cursor c = { data, size, 0 };
    struct hdr_histogram* h = NULL;
    struct hdr_histogram* h2 = NULL;
    struct hdr_interval_recorder rec;
    int have_rec;
    int64_t lowest, highest;
    int significant_figures;
    unsigned ops;

    significant_figures = (int)(u8(&c) % 3) + 1;             /* 1..3 */
    lowest = (int64_t)(u8(&c) % 1000) + 1;                   /* 1..1000 */
    highest = (int64_t)(u64(&c) % (uint64_t)MAX_HIGHEST);
    if (highest < lowest * 2) highest = lowest * 2;

    if (hdr_init(lowest, highest, significant_figures, &h) != 0 || h == NULL)
    {
        return 0;
    }
    /* Second histogram for merge ops; may legitimately fail -> guarded by NULL. */
    hdr_init(lowest, highest, significant_figures, &h2);

    have_rec = (hdr_interval_recorder_init_all(&rec, lowest, highest, significant_figures) == 0);

    for (ops = 0; c.i < c.n && ops < MAX_OPS; ops++)
    {
        uint8_t op = u8(&c);
        int64_t value = (int64_t)(u64(&c) % (uint64_t)(highest + 1));

        switch (op % 12)
        {
        case 0:
            hdr_record_value(h, value);
            break;
        case 1:
            hdr_record_values(h, value, (int64_t)u8(&c) + 1);   /* count 1..256 */
            break;
        case 2:
        {
            /* Bound the co-ordinated-omission backfill to <=256 synthetic
             * entries so a tiny interval can't blow up into a timeout. */
            int64_t interval = value / 256 + 1;
            hdr_record_corrected_value(h, value, interval);
            break;
        }
        case 3:
            (void) hdr_value_at_percentile(h, (double)u8(&c) * 100.0 / 255.0);
            break;
        case 4:
            (void) hdr_count_at_value(h, value);
            (void) hdr_value_at_index(h, (int32_t)(u64(&c) % (uint64_t)(h->counts_len > 0 ? h->counts_len : 1)));
            break;
        case 5:
            (void) hdr_min(h); (void) hdr_max(h);
            (void) hdr_mean(h); (void) hdr_stddev(h);
            break;
        case 6:
        {
            struct hdr_iter iter;
            hdr_iter_percentile_init(&iter, h, (int32_t)u8(&c) + 1);
            drain_iter(&iter);
            break;
        }
        case 7:
        {
            struct hdr_iter iter;
            hdr_iter_recorded_init(&iter, h);
            drain_iter(&iter);
            break;
        }
        case 8:
        {
            struct hdr_iter iter;
            int64_t vpb = (int64_t)(u64(&c) % (uint64_t)(highest + 1)) + 1;  /* >0 */
            hdr_iter_linear_init(&iter, h, vpb);
            drain_iter(&iter);
            break;
        }
        case 9:
            if (h2) (void) hdr_add(h2, h);
            break;
        case 10:
        {
            char* encoded = NULL;
            if (hdr_log_encode(h, &encoded) == 0 && encoded != NULL)
            {
                struct hdr_histogram* decoded = NULL;
                if (hdr_log_decode(&decoded, encoded, strlen(encoded)) == 0 && decoded != NULL)
                {
                    hdr_close(decoded);
                }
                free(encoded);
            }
            break;
        }
        case 11:
            if (have_rec)
            {
                hdr_interval_recorder_record_value(&rec, value);
                (void) hdr_interval_recorder_sample(&rec);
            }
            break;
        default:
            break;
        }
    }

    if (have_rec) hdr_interval_recorder_destroy(&rec);
    if (h2) hdr_close(h2);
    hdr_close(h);
    return 0;
}
