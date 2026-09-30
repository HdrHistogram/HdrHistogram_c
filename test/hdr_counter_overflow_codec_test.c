/* Issue #118: reject unrepresentable V0/V1/V2 count totals.
 * Released to the public domain.
 */
#include <hdr/hdr_histogram.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include "hdr_count_overflow_cases.h"

int main(void)
{
    const int64_t cases[][3] = {
        {1, 2, 3}, {INT64_MAX - 1, 1, 0},
        {INT64_MAX - 1, 2, 0},
        {(int64_t)(UINT64_C(1) << 62), (int64_t)(UINT64_C(1) << 62), (int64_t)(UINT64_C(1) << 62)}
    };
    for (int version = 0; version < 3; version++)
        for (int c = 0; c < 4; c++)
        {
            size_t length = 0;
            uint8_t* frame = count_frame(version, cases[c], &length);
            struct hdr_histogram* h = NULL;
            int rc;
            if (frame == NULL) return 1;
            rc = hdr_decode_compressed(frame, length, &h);
            if (c < 2)
            {
                if (rc != 0 || h == NULL ||
                    h->total_count != (c == 0 ? 6 : INT64_MAX)) return 2;
                hdr_close(h);
            }
            else if (rc != EOVERFLOW || h != NULL) return 3;
            free(frame);
        }
    puts("V0/V1/V2 reject overflowing totals; representable boundary decodes");
    return 0;
}
