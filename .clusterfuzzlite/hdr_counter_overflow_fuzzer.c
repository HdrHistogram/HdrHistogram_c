/* Structured V0/V1/V2 count-total fuzzer. Released to the public domain. */
#include <hdr/hdr_histogram.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include "../test/hdr_count_overflow_cases.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    int64_t counts[3] = {0, 0, 0};
    int64_t total = 0;
    int overflow = 0;
    size_t pos = 1, length = 0;
    uint8_t* frame;
    struct hdr_histogram* h = NULL;
    int rc;
    if (size == 0) return 0;
    for (int i = 0; i < 3; i++)
    {
        uint64_t v = 0;
        for (int j = 0; j < 8; j++)
            v = (v << 8U) | (pos < size ? data[pos++] : 0);
        counts[i] = (int64_t)(v & INT64_MAX);
        if (counts[i] > INT64_MAX - total) overflow = 1;
        else if (!overflow) total += counts[i];
    }
    frame = count_frame(data[0] % 3, counts, &length);
    if (frame == NULL) return 0;
    rc = hdr_decode_compressed(frame, length, &h);
    if (overflow)
    {
        if (rc != EOVERFLOW || h != NULL) abort();
    }
    else if (rc != 0 || h == NULL || h->total_count != total) abort();
    hdr_close(h);
    free(frame);
    return 0;
}
