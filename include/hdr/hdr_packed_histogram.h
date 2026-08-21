/*
 * hdr_packed_histogram.h -- Phase-2 sparse/packed HdrHistogram variant.
 * Released to the public domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 *
 * A memory-optimised histogram whose backing store grows with the number of
 * POPULATED buckets, not with counts_len. Same bucket geometry and identical
 * value<->index mapping as the dense hdr_histogram.
 *
 * Phase-2 vs Phase-1:
 *   - Shared geometry: the config (geometry oracle) is created once and shared
 *     by pointer across many histograms, removing the ~104 B/histogram embedded
 *     dense struct. Dominant fixed-overhead cut for the many-histogram use case.
 *   - Byte-width packed counts: counts are stored at a uniform adaptive width
 *     (1/2/4/8 B) that widens on overflow, so the common "many small counts"
 *     case costs 1-2 B/entry instead of 8.
 *
 * SCOPE: ordered sparse-vector backing (sorted by virtual index) -- memory-
 * optimal for sparse populations. Record is O(log n) search plus, for a NEW
 * bucket, an O(n) memmove insert (so building n distinct buckets in adverse
 * order is O(n^2) worst case); count-width re-pack fires at most 3 times over a
 * histogram's life. A populated-only trie (O(1)-ish access) is deferred: it does
 * not improve memory and is only warranted if access speed becomes a proven
 * bottleneck. See FINDINGS.md.
 *
 * THREAD SAFETY: single-threaded. Unlike the dense library there is NO atomic
 * record variant; concurrent record/record-vs-read on one histogram is a data
 * race (realloc can move/free buffers mid-op -> UAF). The const-qualified query
 * functions AND hdr_packed_encode_compressed (also read-only on the histogram)
 * are safe to call concurrently only when no thread is recording. A
 * shared hdr_packed_config may be READ concurrently by many histograms, but it
 * must outlive every histogram that references it and must not be destroyed
 * while any thread touches such a histogram. External locking is the caller's
 * responsibility. Not async-signal-safe (record may allocate).
 *
 * SERIALIZATION: standard V2 compressed format, interoperable with the dense
 * hdr_encode/decode. Decode REJECTS a stream carrying a non-zero
 * normalizing_index_offset (packed histograms are never rotated) with EINVAL,
 * rather than mis-indexing it; conversion_ratio is not tracked (defaulted to 1).
 *
 * DIVERGENCES FROM DENSE (all intentional; each occurs only where dense is
 * undefined/UB or is a deliberate semantic choice -- packed never returns a
 * silently-wrong answer on a valid in-range input):
 *   1. record_values(count < 0) returns false because packed slots are unsigned
 *      and cannot represent it (dense stores the negative into its int64 slot).
 *   2. record_values that would push total_count past INT64_MAX returns false
 *      with no mutation (dense wraps -- signed-overflow UB).
 *   3. mean()/stddev() on an empty histogram return 0.0 (dense returns NaN from
 *      0/0); and they accumulate in double where dense uses int64, so they can
 *      differ once the running total exceeds 2^53 (double's exact-integer limit)
 *      -- a tiny relative difference (~1e-9), and the only path that stays
 *      well-defined where dense's int64 product would overflow. Not bit-exact.
 *   4. Count sums saturate at INT64_MAX -- in the percentile prefix-sum and in
 *      decode recompute -- instead of overflowing int64 (dense UB). (Record
 *      instead REJECTS a total past INT64_MAX; see item 2.)
 *   5. value_at_percentile guards the count-at-percentile before the int64 cast
 *      (NaN/-inf/negative -> 1; a target that rounds >= total -> total_count) and
 *      clamps the target into [1, total_count], so
 *      the p100 == max invariant always holds. For total_count <= 2^52 this is
 *      bit-for-bit dense. Above 2^52 packed is MORE correct: dense's int64 target
 *      ((int64)((p/100)*total+0.5)) rounds via double (the +0.5 rounds a high
 *      percentile's target UP to total+1 for odd totals starting at 2^52+1, and
 *      the product rounds DOWN for total > 2^53), so dense p100/high-percentiles
 *      can undershoot to an EARLIER bucket (dense p100 != dense max); packed
 *      clamps to the true total and returns the last/max bucket. In the dense-UB
 *      sub-band (total within ~1024 of INT64_MAX) dense's target additionally
 *      casts 2^63->INT64_MIN->1 (UB) and returns the FIRST bucket, while packed
 *      still returns the last/max bucket.
 *   6. decode rejects a stream with a non-zero normalizing_index_offset (EINVAL)
 *      rather than mis-indexing it (packed histograms are never rotated).
 *   7. count_at_value() on an out-of-range value returns 0 (the index is bounds-
 *      checked) where dense reads counts[] out of bounds (UB).
 *   8. value_at_percentiles() (plural) returns the SAME values as the singular
 *      value_at_percentile for every percentile, including the bucket bottom
 *      (lowest-equivalent) at p==0. Dense's SEPARATE plural implementation instead
 *      returns the bucket TOP (highest-equivalent) at p==0 -- a dense singular-vs-
 *      plural inconsistency that packed does not reproduce (packed-plural ==
 *      packed-singular == dense-singular at p0; they agree for all p > 0 within
 *      the bit-for-bit regime total_count <= 2^52, above which high percentiles
 *      follow item 5).
 * For every legitimately-recorded histogram packed is bit-for-bit identical to
 * dense on count_at_value, min, max, total_count, and the V2 encoding. It is also
 * bit-for-bit on value_at_percentile when total_count <= 2^52; above 2^52 packed
 * is more correct (item 5). mean()/stddev() match dense within a small FP
 * tolerance (item 3). All divergences are at a dense-UB or dense-imprecision point.
 *
 * NOTE: memory feature -- higher per-record cost than dense. Not for an ops/sec
 * hot path.
 */
#ifndef HDR_PACKED_HISTOGRAM_H
#define HDR_PACKED_HISTOGRAM_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct hdr_packed_config;    /* opaque, shared geometry -- create once */
struct hdr_packed_histogram; /* opaque */

/* Shared geometry. One config can back any number of histograms. */
int  hdr_packed_config_create(int64_t lowest_discernible_value,
                              int64_t highest_trackable_value,
                              int significant_figures,
                              struct hdr_packed_config** result);
/* Must not be called while any histogram created from this config is still
   alive or in use by any thread (no refcounting -- caller-enforced lifetime). */
void hdr_packed_config_destroy(struct hdr_packed_config* cfg);
size_t hdr_packed_config_memory_size(const struct hdr_packed_config* cfg);

/* Histogram referencing a shared config (config must outlive the histogram). */
int  hdr_packed_init_shared(const struct hdr_packed_config* cfg,
                            struct hdr_packed_histogram** result);

/* Convenience: histogram owning a private config (standalone use). */
int  hdr_packed_init(int64_t lowest_discernible_value,
                     int64_t highest_trackable_value,
                     int significant_figures,
                     struct hdr_packed_histogram** result);

void hdr_packed_close(struct hdr_packed_histogram* h);

bool hdr_packed_record_value(struct hdr_packed_histogram* h, int64_t value);
bool hdr_packed_record_values(struct hdr_packed_histogram* h, int64_t value, int64_t count);

/* Empties the histogram but RETAINS the allocated arrays and the current count
   width (reclaimed only by hdr_packed_close), mirroring dense hdr_reset which
   keeps counts[]. No realloc, so a record->snapshot->reset loop does not thrash. */
void    hdr_packed_reset(struct hdr_packed_histogram* h);
int64_t hdr_packed_total_count(const struct hdr_packed_histogram* h);
int64_t hdr_packed_min(const struct hdr_packed_histogram* h);
int64_t hdr_packed_max(const struct hdr_packed_histogram* h);
double  hdr_packed_mean(const struct hdr_packed_histogram* h);
double  hdr_packed_stddev(const struct hdr_packed_histogram* h);
int64_t hdr_packed_count_at_value(const struct hdr_packed_histogram* h, int64_t value);
int64_t hdr_packed_value_at_percentile(const struct hdr_packed_histogram* h, double percentile);
int     hdr_packed_value_at_percentiles(const struct hdr_packed_histogram* h,
            const double* percentiles, int64_t* values, size_t length);

/* Bytes held by this histogram (struct + live sparse arrays; plus the private
   config if it owns one -- a shared config is excluded, count it once via
   hdr_packed_config_memory_size). */
size_t  hdr_packed_get_memory_size(const struct hdr_packed_histogram* h);
int32_t hdr_packed_populated(const struct hdr_packed_histogram* h);
int     hdr_packed_count_width(const struct hdr_packed_histogram* h); /* 1/2/4/8 */

/* ---- V2 serialization (interop with the dense hdr_encode/decode) ----------
 * hdr_packed_encode_compressed streams the standard V2 compressed format
 * directly from the sparse backing (no dense array is ever materialized). The
 * bytes are byte-identical to hdr_encode_compressed on an equivalent dense
 * histogram, so the dense decoder reads them back exactly, and vice versa.
 * Caller frees *compressed_histogram with free(). */
int hdr_packed_encode_compressed(
    const struct hdr_packed_histogram* h, uint8_t** compressed_histogram, size_t* compressed_len);

/* Decode a standard V2 compressed stream into a new packed histogram (owns its
 * config). Accepts streams produced by either encoder. */
int hdr_packed_decode_compressed(
    uint8_t* compressed_histogram, size_t length, struct hdr_packed_histogram** result);

#endif
