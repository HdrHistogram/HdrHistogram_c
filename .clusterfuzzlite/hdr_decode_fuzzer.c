/*
 * Fuzzer for the raw histogram decode path: base64 -> gzip inflate -> binary
 * V0/V1/V2 payload decode (hdr_encoding.c / hdr_histogram_log.c). This targets
 * the parsing/decompression code directly with attacker-controlled bytes,
 * complementing the CSV log-reader fuzzer.
 *
 * Released to the public domain.
 */
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_histogram_log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    struct hdr_histogram* h = NULL;
    char* buf;

    if (size == 0)
    {
        return 0;
    }

    /* hdr_log_decode takes a (base64) char buffer + length; give it a private,
     * NUL-terminated copy so we never read past the fuzz input. */
    buf = (char*) malloc(size + 1);
    if (buf == NULL)
    {
        return 0;
    }
    memcpy(buf, data, size);
    buf[size] = '\0';

    if (hdr_log_decode(&h, buf, size) == 0 && h != NULL)
    {
        /* Touch the decoded histogram so any inconsistent internal state
         * (e.g. bad counts_len vs. allocation) is exercised. */
        (void) hdr_value_at_percentile(h, 99.0);
        (void) hdr_max(h);
        (void) h->total_count;
        hdr_close(h);
    }

    free(buf);
    return 0;
}
