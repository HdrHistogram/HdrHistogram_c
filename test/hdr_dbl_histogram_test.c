/**
 * hdr_dbl_histogram_test.c
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <hdr_dbl_histogram.h>

#include "minunit.h"

int tests_run = 0;

const int64_t TRACKABLE_VALUE_RANGE_SIZE = 3600L * 1000 * 1000; // e.g. for 1 hr in usec units
const int32_t SIGNIFICANT_FIGURES = 3;

char* test_construct_argument_ranges()
{
    struct hdr_dbl_histogram* h = NULL;

    mu_assert("highest_to_lowest_value_ratio must be >= 2", 0 != hdr_dbl_init(1, SIGNIFICANT_FIGURES, &h));
    mu_assert("significant_figures must be > 0", 0 != hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, -1, &h));
    mu_assert(
            "(highest_to_lowest_value_ratio * 10^significant_figures) must be < (1L << 61)",
            0 != hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, 6, &h));

    return 0;
}

char* test_construction_argument_gets()
{
    struct hdr_dbl_histogram* h;

    mu_assert("Should construct", 0 == hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h));
    mu_assert("Should record", hdr_dbl_record_value(h, pow(2.0, 20)));
    mu_assert("Should record", hdr_dbl_record_value(h, 1.0));

    mu_assert("Significant figures", SIGNIFICANT_FIGURES == h->values.significant_figures);
    mu_assert("Significant figures", TRACKABLE_VALUE_RANGE_SIZE == h->highest_to_lowest_value_ratio);
    mu_assert("Lowest value should be correct", compare_double(1.0, h->current_lowest_value, 0.001));

    return 0;
}

char* test_data_range()
{
    struct hdr_dbl_histogram* h;

    mu_assert("Should construct", 0 == hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h));

    hdr_dbl_record_value(h, 0.0);

    mu_assert("Should handle 0.0 value", 1 == hdr_count_at_value(&h->values, 0.0));

    double top_value = 1.0;
    while (hdr_dbl_record_value(h, top_value))
    {
        top_value *= 2.0;
    }
    mu_assert("Top value should be roughly 2^33", compare_double(INT64_C(1) << 33, top_value, 0.00001));
    mu_assert("Should only be 1 value at 0.0", INT64_C(1) == hdr_count_at_value(&h->values, 0.0));

    free(h);

    mu_assert("Should construct", 0 == hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h));
    hdr_dbl_record_value(h, 0.0);

    double bottom_value = INT64_C(1) << 33;
    while (hdr_dbl_record_value(h, bottom_value))
    {
        bottom_value /= 2.0;
    }

    int64_t expected_range = INT64_C(1) << ((64 - __builtin_clzll(TRACKABLE_VALUE_RANGE_SIZE)) + 1);

    mu_assert("Range should be same as top/bottom", compare_double(expected_range, top_value/bottom_value, 0.00001));
    mu_assert("Bottom value should be around 1.0", compare_double(1.0, bottom_value, 0.00001));
    mu_assert("Should only be 1 value at 0.0", INT64_C(1) == hdr_count_at_value(&h->values, 0.0));

    return 0;
}

static struct mu_result all_tests()
{
    mu_run_test(test_construct_argument_ranges);
    mu_run_test(test_construction_argument_gets);
    mu_run_test(test_data_range);

    mu_ok;
}

int hdr_dbl_histogram_run_tests()
{
    struct mu_result result = all_tests();

    if (result.message != 0)
    {
        printf("hdr_histogram_log_test.%s(): %s\n", result.test, result.message);
    }
    else
    {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return (int) result.message;
}

int main(int argc, char **argv)
{
    return hdr_dbl_histogram_run_tests();
}
