/*
 * hdr_packed_histogram_no_op.c -- no-op packed variant for builds without zlib.
 * Released to the public domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Compiled instead of hdr_packed_histogram.c when the log/zlib codec is disabled
 * (HDR_LOG_REQUIRED=DISABLED / zlib absent). Every exported hdr_packed_* symbol
 * still resolves so callers link, but constructors fail with ENOMEM and the
 * queries return neutral values -- mirroring hdr_histogram_log_no_op.c.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>

#include <hdr/hdr_packed_histogram.h>

#define UNUSED(x) (void)(x)

int hdr_packed_config_create(
    int64_t lowest_discernible_value, int64_t highest_trackable_value,
    int significant_figures, struct hdr_packed_config** result)
{
    UNUSED(lowest_discernible_value); UNUSED(highest_trackable_value);
    UNUSED(significant_figures); UNUSED(result);
    return ENOMEM;
}

void hdr_packed_config_destroy(struct hdr_packed_config* cfg)
{
    UNUSED(cfg);
}

size_t hdr_packed_config_memory_size(const struct hdr_packed_config* cfg)
{
    UNUSED(cfg);
    return 0;
}

int hdr_packed_init_shared(
    const struct hdr_packed_config* cfg, struct hdr_packed_histogram** result)
{
    UNUSED(cfg); UNUSED(result);
    return ENOMEM;
}

int hdr_packed_init(
    int64_t lowest_discernible_value, int64_t highest_trackable_value,
    int significant_figures, struct hdr_packed_histogram** result)
{
    UNUSED(lowest_discernible_value); UNUSED(highest_trackable_value);
    UNUSED(significant_figures); UNUSED(result);
    return ENOMEM;
}

void hdr_packed_close(struct hdr_packed_histogram* h)
{
    UNUSED(h);
}

bool hdr_packed_record_value(struct hdr_packed_histogram* h, int64_t value)
{
    UNUSED(h); UNUSED(value);
    return false;
}

bool hdr_packed_record_values(struct hdr_packed_histogram* h, int64_t value, int64_t count)
{
    UNUSED(h); UNUSED(value); UNUSED(count);
    return false;
}

void hdr_packed_reset(struct hdr_packed_histogram* h)
{
    UNUSED(h);
}

int64_t hdr_packed_total_count(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0;
}

int64_t hdr_packed_min(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return INT64_MAX;
}

int64_t hdr_packed_max(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0;
}

double hdr_packed_mean(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0.0;
}

double hdr_packed_stddev(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0.0;
}

int64_t hdr_packed_count_at_value(const struct hdr_packed_histogram* h, int64_t value)
{
    UNUSED(h); UNUSED(value);
    return 0;
}

int64_t hdr_packed_value_at_percentile(const struct hdr_packed_histogram* h, double percentile)
{
    UNUSED(h); UNUSED(percentile);
    return 0;
}

int hdr_packed_value_at_percentiles(const struct hdr_packed_histogram* h,
    const double* percentiles, int64_t* values, size_t length)
{
    UNUSED(h); UNUSED(percentiles); UNUSED(values); UNUSED(length);
    return -1;
}

size_t hdr_packed_get_memory_size(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0;
}

int32_t hdr_packed_populated(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0;
}

int hdr_packed_count_width(const struct hdr_packed_histogram* h)
{
    UNUSED(h);
    return 0;
}

int hdr_packed_encode_compressed(
    const struct hdr_packed_histogram* h, uint8_t** compressed_histogram, size_t* compressed_len)
{
    UNUSED(h); UNUSED(compressed_histogram); UNUSED(compressed_len);
    return -1;
}

int hdr_packed_decode_compressed(
    uint8_t* compressed_histogram, size_t length, struct hdr_packed_histogram** result)
{
    UNUSED(compressed_histogram); UNUSED(length); UNUSED(result);
    return -1;
}
