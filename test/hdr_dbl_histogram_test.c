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

const int64_t TRACKABLE_VALUE_RANGE_SIZE = INT64_C(3600) * 1000 * 1000; // e.g. for 1 hr in usec units
const int32_t SIGNIFICANT_FIGURES = 3;
const double TEST_VALUE_LEVEL = 4.0;

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
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, 0.0);

    mu_assert("Should handle 0.0 value", 1 == hdr_count_at_value(&h->values, 0.0));

    double top_value = 1.0;
    while (hdr_dbl_record_value(h, top_value))
    {
        top_value *= 2.0;
    }
    mu_assert("Top value should be roughly 2^33", compare_double(INT64_C(1) << 33, top_value, 0.00001));
    mu_assert("Should only be 1 value at 0.0", 1 == hdr_count_at_value(&h->values, 0.0));

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
    mu_assert("Should only be 1 value at 0.0", 1 == hdr_count_at_value(&h->values, 0.0));

    return 0;
}

char* test_record_value()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, TEST_VALUE_LEVEL);
    mu_assert("Count at value not 1", compare_int64(1, hdr_dbl_count_at_value(h, TEST_VALUE_LEVEL)));
    mu_assert("Total count not 1", compare_int64(1, h->values.total_count));

    return 0;
}

char* test_record_value_overflow()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, TRACKABLE_VALUE_RANGE_SIZE * 3);
    mu_assert("Should not record if overflow will occur", !hdr_dbl_record_value(h, 1.0));

    return 0;
}

char* test_record_value_with_expected_interval()
{
    struct hdr_dbl_histogram* raw_histogram;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &raw_histogram);

    hdr_dbl_record_value(raw_histogram, 0);
    hdr_dbl_record_value(raw_histogram, TEST_VALUE_LEVEL);

    mu_assert("Raw Count at 0 should be 1", compare_int64(1, hdr_dbl_count_at_value(raw_histogram, 0.0)));
    mu_assert(
            "Raw should not contain corrected value",
            compare_int64(0, hdr_dbl_count_at_value(raw_histogram, (TEST_VALUE_LEVEL * 1) / 4)));
    mu_assert(
            "Raw should not contain corrected value",
            compare_int64(0, hdr_dbl_count_at_value(raw_histogram, (TEST_VALUE_LEVEL * 2) / 4)));
    mu_assert(
            "Raw should not contain corrected value",
            compare_int64(0, hdr_dbl_count_at_value(raw_histogram, (TEST_VALUE_LEVEL * 3) / 4)));
    mu_assert(
            "Raw should not contain corrected value",
            compare_int64(1, hdr_dbl_count_at_value(raw_histogram, (TEST_VALUE_LEVEL * 4) / 4)));
    mu_assert("Count should be 2", compare_int64(2, raw_histogram->values.total_count));

    struct hdr_dbl_histogram* cor_histogram;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &cor_histogram);

    hdr_dbl_record_value(cor_histogram, 0);
    hdr_dbl_record_corrected_value(cor_histogram, TEST_VALUE_LEVEL, TEST_VALUE_LEVEL / 4);

    mu_assert("Corrected Count at 0 should be 1", compare_int64(1, hdr_dbl_count_at_value(cor_histogram, 0.0)));
    mu_assert(
            "Should contain corrected value",
            compare_int64(1, hdr_dbl_count_at_value(cor_histogram, (TEST_VALUE_LEVEL * 1) / 4)));
    mu_assert(
            "Should contain corrected value",
            compare_int64(1, hdr_dbl_count_at_value(cor_histogram, (TEST_VALUE_LEVEL * 2) / 4)));
    mu_assert(
            "Should contain corrected value",
            compare_int64(1, hdr_dbl_count_at_value(cor_histogram, (TEST_VALUE_LEVEL * 3) / 4)));
    mu_assert(
            "Should contain corrected value",
            compare_int64(1, hdr_dbl_count_at_value(cor_histogram, (TEST_VALUE_LEVEL * 4) / 4)));
    mu_assert("Count should be 5", compare_int64(5, cor_histogram->values.total_count));

    return 0;
}

char* test_reset()
{
    struct hdr_dbl_histogram* h;
    mu_assert("Should construct", 0 == hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h));

    hdr_dbl_record_value(h, TEST_VALUE_LEVEL);
    hdr_dbl_reset(h);

    mu_assert("Should have 0 count", compare_int64(0, hdr_dbl_count_at_value(h, TEST_VALUE_LEVEL)));
    mu_assert("Should have 0 total", compare_int64(0, h->values.total_count));

    return 0;
}

char* test_add()
{
    struct hdr_dbl_histogram* h1;
    struct hdr_dbl_histogram* h2;

    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h1);
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h2);

    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL * 1000);

    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL * 1000);

    mu_assert("Should not drop values", 0 == hdr_dbl_add(h1, h2));

    mu_assert("Should have count of 2", compare_int64(2, hdr_dbl_count_at_value(h1, TEST_VALUE_LEVEL)));
    mu_assert("Should have count of 2", compare_int64(2, hdr_dbl_count_at_value(h1, TEST_VALUE_LEVEL * 1000)));
    mu_assert("Should have total of 4", compare_int64(4, h1->values.total_count));

    return 0;
}

char* test_add_smaller_to_bigger()
{
    struct hdr_dbl_histogram* h1;
    struct hdr_dbl_histogram* h2;

    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE * 2, SIGNIFICANT_FIGURES, &h1);
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h2);

    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL * 1000);

    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL * 1000);

    mu_assert("Should not drop values", 0 == hdr_dbl_add(h1, h2));

    mu_assert("Should have count of 2", compare_int64(2, hdr_dbl_count_at_value(h1, TEST_VALUE_LEVEL)));
    mu_assert("Should have count of 2", compare_int64(2, hdr_dbl_count_at_value(h1, TEST_VALUE_LEVEL * 1000)));
    mu_assert("Should have total of 4", compare_int64(4, h1->values.total_count));

    return 0;
}

/*
        DoubleHistogram biggerOther = new DoubleHistogram(trackableValueRangeSize * 2, numberOfSignificantValueDigits);
        biggerOther.recordValue(testValueLevel);
        biggerOther.recordValue(testValueLevel * 1000);

        // Adding the smaller histogram to the bigger one should work:
        biggerOther.add(histogram);
        assertEquals(3L, biggerOther.getCountAtValue(testValueLevel));
        assertEquals(3L, biggerOther.getCountAtValue(testValueLevel * 1000));
        assertEquals(6L, biggerOther.getTotalCount());

        // Since we are auto-sized, trying to add a larger histogram into a smaller one should work if no
        // overflowing data is there:
        try {
            // This should throw:
            histogram.add(biggerOther);
        } catch (ArrayIndexOutOfBoundsException e) {
            fail("Should of thown with out of bounds error");
        }

        // But trying to add smaller values to a larger histogram that actually uses it's range should throw an AIOOB:
        histogram.recordValue(1.0);
        other.recordValue(1.0);
        biggerOther.recordValue(trackableValueRangeSize * 8);

        try {
            // This should throw:
            biggerOther.add(histogram);
            fail("Should of thown with out of bounds error");
        } catch (ArrayIndexOutOfBoundsException e) {
        }
    }
*/
char* test_add_bigger_to_smaller_out_of_range()
{
    struct hdr_dbl_histogram* h1;
    struct hdr_dbl_histogram* h2;

    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h1);
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE * 2, SIGNIFICANT_FIGURES, &h2);

    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h1, TEST_VALUE_LEVEL * 1000);
    hdr_dbl_record_value(h1, 1.0);

    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL);
    hdr_dbl_record_value(h2, TEST_VALUE_LEVEL * 1000);
    hdr_dbl_record_value(h2, 1.0);

    mu_assert("Should not drop values", 0 == hdr_dbl_add(h1, h2));

    // TODO: To make this work correctly we need auto-resizing.
//    hdr_dbl_record_value(this, 1.0);
//    hdr_dbl_record_value(that, 1.0);
//
//    hdr_dbl_record_value(that, TRACKABLE_VALUE_RANGE_SIZE * 8); // Force resize
//
//    mu_assert("Should fail to add", hdr_dbl_add(that, this));

    return 0;
}

char* test_size_of_equivalent_value_range()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, 1.0);

    mu_assert(
        "Size of equivalent range for value 1 is 1",
        compare_double(1.0/1024.0, hdr_dbl_size_of_equivalent_value_range(h, 1), 0.001));
    mu_assert(
        "Size of equivalent range for value 2500 is 2",
        compare_double(2.0, hdr_dbl_size_of_equivalent_value_range(h, 2500), 0.001));
    mu_assert(
        "Size of equivalent range for value 8191 is 4",
        compare_double(4.0, hdr_dbl_size_of_equivalent_value_range(h, 8191), 0.001));
    mu_assert(
        "Size of equivalent range for value 8192 is 8",
        compare_double(8.0, hdr_dbl_size_of_equivalent_value_range(h, 8192), 0.001));
    mu_assert(
        "Size of equivalent range for value 10000 is 8",
        compare_double(8.0, hdr_dbl_size_of_equivalent_value_range(h, 10000), 0.001));

    return 0;
}

char* test_lowest_equivalent_value()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, 1.0);
    mu_assert(
        "The lowest equivalent value to 10007 is 10000",
        compare_double(10000, hdr_dbl_lowest_equivalent_value(h, 10007), 0.001));
    mu_assert(
        "The lowest equivalent value to 10008 is 10009",
        compare_double(10008, hdr_dbl_lowest_equivalent_value(h, 10009), 0.001));

    return 0;
}

char* test_highest_equivalent_value()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);
    hdr_dbl_record_value(h, 1.0);
    mu_assert(
        "The highest equivalent value for 8180 is ~8184",
        compare_double(8183.99999, hdr_dbl_highest_equivalent_value(h, 8180), 0.001));
    mu_assert(
        "The highest equivalent value for 8191 is ~8192",
        compare_double(8191.99999, hdr_dbl_highest_equivalent_value(h, 8191), 0.001));
    mu_assert(
        "The highest equivalent value for 8193 is ~8200",
        compare_double(8199.99999, hdr_dbl_highest_equivalent_value(h, 8193), 0.001));
    mu_assert(
        "The highest equivalent value for 9995 is ~10000",
        compare_double(9999.99999, hdr_dbl_highest_equivalent_value(h, 9995), 0.001));
    mu_assert(
        "The highest equivalent value for 10007 is ~10008",
        compare_double(10007.99999, hdr_dbl_highest_equivalent_value(h, 10007), 0.001));
    mu_assert(
        "The highest equivalent value for 10008 is ~10016",
        compare_double(10015.99999, hdr_dbl_highest_equivalent_value(h, 10008), 0.001));

    return 0;
}

char* test_median_equivalent_value()
{
    struct hdr_dbl_histogram* h;
    hdr_dbl_init(TRACKABLE_VALUE_RANGE_SIZE, SIGNIFICANT_FIGURES, &h);

    hdr_dbl_record_value(h, 1.0);
    mu_assert(
        "The median equivalent value for 4 is 4.002",
        compare_double(4.002, hdr_dbl_median_equivalent_value(h, 4), 0.001));
    mu_assert(
        "The median equivalent value for 5 is 5.002",
        compare_double(5.002, hdr_dbl_median_equivalent_value(h, 5), 0.001));
    mu_assert(
        "The median equivalent value for 4000 is 4001",
        compare_double(4001, hdr_dbl_median_equivalent_value(h, 4000), 0.001));
    mu_assert(
        "The median equivalent value for 8000 is 8002",
        compare_double(8002, hdr_dbl_median_equivalent_value(h, 8000), 0.001));
    mu_assert(
        "The median equivalent value for 10007 is 10004",
        compare_double(10004, hdr_dbl_median_equivalent_value(h, 10007), 0.001));

    return 0;
}

static struct mu_result all_tests()
{
    mu_run_test(test_construct_argument_ranges);
    mu_run_test(test_construction_argument_gets);
    mu_run_test(test_data_range);
    mu_run_test(test_record_value);
    mu_run_test(test_record_value_overflow);
    mu_run_test(test_record_value_with_expected_interval);
    mu_run_test(test_reset);
    mu_run_test(test_add);
    mu_run_test(test_add_smaller_to_bigger);
    mu_run_test(test_add_bigger_to_smaller_out_of_range);
    mu_run_test(test_size_of_equivalent_value_range);
    mu_run_test(test_lowest_equivalent_value);
    mu_run_test(test_highest_equivalent_value);
    mu_run_test(test_median_equivalent_value);

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
