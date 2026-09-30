/* Issue #118: imported positive counts can exceed INT64_MAX.
 * Released to the public domain.
 */
#include <hdr/hdr_histogram.h>
#include "hdr_tests.h"
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    struct hdr_histogram* h = NULL;
    if (hdr_init(1, 1000, 1, &h)) return 1;
    h->counts[1] = INT64_MAX - 1;
    h->counts[2] = 1;
    if (!hdr_reset_internal_counters_checked(h) || h->total_count != INT64_MAX) return 2;
    h->counts[2] = 2;
    if (hdr_reset_internal_counters_checked(h) || h->total_count != INT64_MAX) return 3;
    hdr_reset_internal_counters(h);
    if (h->total_count != INT64_MAX) return 4;
    h->counts[1] = (int64_t)(UINT64_C(1) << 62);
    h->counts[2] = (int64_t)(UINT64_C(1) << 62);
    h->counts[3] = (int64_t)(UINT64_C(1) << 62);
    if (hdr_reset_internal_counters_checked(h) || h->total_count != INT64_MAX) return 5;
    hdr_reset(h);
    h->normalizing_index_offset = 37;
    for (int logical = 1; logical <= 3; logical++)
    {
        int32_t physical = (logical - 37 + h->counts_len) % h->counts_len;
        h->counts[physical] = (int64_t)(UINT64_C(1) << 62);
    }
    if (hdr_reset_internal_counters_checked(h) || h->total_count != INT64_MAX ||
        hdr_min(h) != hdr_value_at_index(h, 1) ||
        hdr_max(h) != hdr_value_at_index(h, 3)) return 6;
    hdr_close(h);
    puts("counter rebuild saturates without signed overflow");
    return 0;
}
