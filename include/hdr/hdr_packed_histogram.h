/*
 * Sparse histogram with shared geometry and adaptive 1/2/4/8-byte counts.
 * Released to the public domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Storage grows with populated buckets. Recording uses binary search and an
 * O(n) insertion for new buckets; this trades recording speed for memory.
 * Reset retains allocated storage and count width.
 *
 * Concurrent reads require no concurrent mutation. There is no atomic recording
 * API. A shared config must outlive every histogram referencing it; callers
 * provide synchronization and ownership management.
 *
 * Negative counts and recordings that overflow total_count are rejected.
 * Empty mean/stddev return zero. Percentile targets are clamped to the total;
 * NaN and positive infinity select p100, negative percentiles select count one.
 * The plural API accepts unsorted percentiles and agrees with singular queries,
 * including the lowest equivalent value at p0. Floating-point statistics may
 * differ from dense statistics due to accumulation order.
 *
 * Serialization uses standard V2 compressed bytes for equivalent distributions.
 * Decode rejects nonzero normalizing offsets and defaults conversion_ratio to
 * one. Decoded count sums saturate at INT64_MAX; queries on such overflowing
 * distributions do not provide exact ranks. Codec calls return -1 when logging
 * is disabled; recording and queries remain available.
 */
#ifndef HDR_PACKED_HISTOGRAM_H
#define HDR_PACKED_HISTOGRAM_H 1

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
