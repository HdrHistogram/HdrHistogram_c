/* Packed/dense differential recording and hostile V2 decoding.
 * Released to the public domain.
 */
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_histogram_log.h>
#include <hdr/hdr_packed_histogram.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Internal codec entry point, also used by the codec tests. */
extern int hdr_encode_compressed(struct hdr_histogram*, uint8_t**, size_t*);

static void check_decode(uint8_t* data, size_t size)
{
    struct hdr_packed_histogram* h = NULL;
    if (hdr_packed_decode_compressed(data, size, &h) == 0)
    {
        (void) hdr_packed_min(h);
        (void) hdr_packed_max(h);
        (void) hdr_packed_mean(h);
        (void) hdr_packed_stddev(h);
        (void) hdr_packed_value_at_percentile(h, 100.0);
        hdr_packed_close(h);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    struct hdr_histogram* dense = NULL;
    struct hdr_packed_histogram* packed = NULL;
    uint8_t *encoded = NULL, *reference = NULL, *copy;
    size_t encoded_size = 0, reference_size = 0;
    const double percentiles[] = {0, 1, 50, 99, 100};
    int64_t high;
    if (size == 0) return 0;
    copy = (uint8_t*) malloc(size);
    if (copy == NULL) return 0;
    memcpy(copy, data, size);
    check_decode(copy, size);
    free(copy);

    high = (data[0] & 1) ? INT64_MAX : 3600000000LL;
    if (hdr_init(1, high, 3, &dense) != 0) return 0;
    if (hdr_packed_init(1, high, 3, &packed) != 0)
    {
        hdr_close(dense);
        return 0;
    }
    /* Leave half the input for mutations. Totals stay below 2^52. */
    size_t pos = 1;
    for (size_t n = 0; n < 128 && pos + 9 <= size / 2; n++)
    {
        uint64_t value = 0;
        for (int j = 0; j < 8; j++) value = (value << 8) | data[pos++];
        int64_t count = (int64_t) ((uint64_t) 1 << (data[pos++] % 41));
        int64_t sample = (int64_t) (value % (uint64_t) high);
        if (hdr_record_values(dense, sample, count) !=
            hdr_packed_record_values(packed, sample, count)) abort();
    }
    if (dense->total_count != hdr_packed_total_count(packed) ||
        hdr_min(dense) != hdr_packed_min(packed) ||
        hdr_max(dense) != hdr_packed_max(packed)) abort();
    for (size_t j = 0; j < sizeof(percentiles) / sizeof(percentiles[0]); j++)
        if (hdr_value_at_percentile(dense, percentiles[j]) !=
            hdr_packed_value_at_percentile(packed, percentiles[j])) abort();
    if (hdr_packed_encode_compressed(packed, &encoded, &encoded_size) == 0)
    {
        if (hdr_encode_compressed(dense, &reference, &reference_size) == 0 &&
            (encoded_size != reference_size ||
             memcmp(encoded, reference, encoded_size) != 0)) abort();
        check_decode(encoded, encoded_size);
        for (size_t j = 0; j < encoded_size && pos < size; j++, pos++)
            if (data[pos] & 1) encoded[j] ^= data[pos];
        check_decode(encoded, encoded_size);
    }
    free(encoded);
    free(reference);
    hdr_packed_close(packed);
    hdr_close(dense);
    return 0;
}
