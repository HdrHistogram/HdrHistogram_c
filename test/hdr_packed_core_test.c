/* Released to the public domain under CC0. Runs with and without logging. */
#include <hdr/hdr_packed_histogram.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    struct hdr_packed_histogram* h = NULL;
    int result = hdr_packed_init(1, 1000000, 3, &h);
    if (result) return 1;
    if (!hdr_packed_record_value(h, 1234) ||
        !hdr_packed_record_values(h, 4321, 65536) ||
        hdr_packed_total_count(h) != 65537 ||
        hdr_packed_count_at_value(h, 4321) != 65536 ||
        hdr_packed_count_at_value(h, -1) != 0 ||   /* negative value must return 0, not a mis-mapped bucket */
        hdr_packed_value_at_percentile(h, 100) != hdr_packed_max(h))
    {
        hdr_packed_close(h);
        return 1;
    }
    hdr_packed_reset(h);
    result = hdr_packed_total_count(h) != 0;
    hdr_packed_close(h);
    puts("packed core available and correct");
    return result;
}
