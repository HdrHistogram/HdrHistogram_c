#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <hdr_histogram.h>

int main(int argc, char** argv)
{
    struct hdr_histogram* h;

    for (int i = 1; i <= 5; i++)
    {
        hdr_init(1, 100000, i, &h);

        printf(
            "significant_figures: %"PRId64
            ", sub_bucket_half_count_magnitude: %d"
            ", sub_bucket_half_count: %d"
            ", sub_bucket_mask: %"PRId64
            ", sub_bucket_count: %d"
            ", bucket_count: %d\n",
            h->significant_figures, h->sub_bucket_half_count_magnitude, h->sub_bucket_half_count, h->sub_bucket_mask, h->sub_bucket_count, h->bucket_count);
    }

}