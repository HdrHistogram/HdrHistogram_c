/**
 * hdr_dbl_histogram.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_DBL_HISTOGRAM_H
#define HDR_DBL_HISTOGRAM_H 1

#include <stdint.h>
#include "hdr_histogram.h"

struct hdr_dbl_histogram
{
    double current_lowest_value;
    double current_highest_value;
    int64_t highest_to_lowest_value_ratio;
    double int_to_dbl_conversion_ratio;
    double dbl_to_int_conversion_ratio;

    struct hdr_histogram values;
};

int hdr_dbl_init(
    int64_t highest_to_lowest_value_ratio,
    int32_t significant_figures,
    struct hdr_dbl_histogram** result);

bool hdr_dbl_record_value(struct hdr_dbl_histogram* h, double value);
bool hdr_dbl_record_values(struct hdr_dbl_histogram* h, double value, int64_t count);
bool hdr_dbl_record_corrected_value(struct hdr_dbl_histogram* h, double value, double expected_interval);
bool hdr_dbl_record_corrected_values(struct hdr_dbl_histogram* h, double value, int64_t count, double expected_interval);
double hdr_dbl_size_of_equivalent_value_range(struct hdr_dbl_histogram* h, double value);
double hdr_dbl_lowest_equivalent_value(struct hdr_dbl_histogram* h, double value);
double hdr_dbl_highest_equivalent_value(struct hdr_dbl_histogram* h, double value);
double hdr_dbl_median_equivalent_value(struct hdr_dbl_histogram* h, double value);
int64_t hdr_dbl_add_while_correcting_for_coordinated_omission(
        struct hdr_dbl_histogram** dest,
        struct hdr_dbl_histogram* src,
        double expected_interval);
double hdr_dbl_mean(struct hdr_dbl_histogram* h);
double hdr_dbl_value_at_percentile(struct hdr_dbl_histogram* h, double percentile);
double hdr_dbl_max(struct hdr_dbl_histogram* h);

/**
 * Add the values from the addend histogram to the sum histogram.
 */
int64_t hdr_dbl_add(struct hdr_dbl_histogram* sum, struct hdr_dbl_histogram* addend);
void hdr_dbl_reset(struct hdr_dbl_histogram* h);

int64_t hdr_dbl_count_at_value(struct hdr_dbl_histogram* h, double value);

#endif