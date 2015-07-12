#include <stdlib.h>
#include <stdio.h>

#include <hdr_dbl_histogram.h>

#include "minunit.h"

int tests_run = 0;

const int64_t HIGHEST_TRACKABLE_VALUE = 3600L * 1000 * 1000; // 1 hour in usec units
const int32_t SIGNIFICANT_FIGURES = 3; // Maintain at least 3 decimal points of accuracy

static struct hdr_dbl_histogram* raw_histogram = NULL;
static struct hdr_dbl_histogram* histogram = NULL;
static struct hdr_dbl_histogram* scaled_raw_histogram = NULL;
static struct hdr_dbl_histogram* scaled_histogram = NULL;
static struct hdr_dbl_histogram* post_corrected_histogram = NULL;
static struct hdr_dbl_histogram* post_corrected_scaled_histogram = NULL;

static void do_free(struct hdr_dbl_histogram** h)
{
    free(*h);
    *h = NULL;
}

void free_histograms()
{
    do_free(&raw_histogram);
    do_free(&histogram);
    do_free(&scaled_raw_histogram);
    do_free(&scaled_histogram);
    do_free(&post_corrected_histogram);
    do_free(&post_corrected_scaled_histogram);
}

void load_histograms()
{
    free_histograms();

    hdr_dbl_init(HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_FIGURES, &histogram);
    hdr_dbl_init(HIGHEST_TRACKABLE_VALUE / 2, SIGNIFICANT_FIGURES, &scaled_histogram);
    hdr_dbl_init(HIGHEST_TRACKABLE_VALUE, SIGNIFICANT_FIGURES, &raw_histogram);
    hdr_dbl_init(HIGHEST_TRACKABLE_VALUE / 2, SIGNIFICANT_FIGURES, &scaled_raw_histogram);

    for (int i = 0; i < 10000; i++)
    {
        hdr_dbl_record_corrected_value(histogram, 1000, 10000);
        hdr_dbl_record_corrected_value(scaled_histogram, 1000 * 512, 10000 * 512);
        hdr_dbl_record_value(raw_histogram, INT64_C(1000));
        hdr_dbl_record_value(scaled_raw_histogram, 1000 * 512);
    }
    hdr_dbl_record_corrected_value(histogram, INT64_C(100000000), 10000);
    hdr_dbl_record_corrected_value(scaled_histogram, INT64_C(100000000) * 512, 10000 * 512);
    hdr_dbl_record_value(raw_histogram, INT64_C(100000000));
    hdr_dbl_record_value(scaled_raw_histogram, INT64_C(100000000) * 512);

    hdr_dbl_add_while_correcting_for_coordinated_omission(
        &post_corrected_histogram, raw_histogram, 10000);
    hdr_dbl_add_while_correcting_for_coordinated_omission(
        &post_corrected_scaled_histogram, scaled_raw_histogram, 512 * 10000);
}

char *test_scaling_equivalence() {
    load_histograms();

    mu_assert("total count should be the same",
        compare_int64(histogram->values.total_count, scaled_histogram->values.total_count));

    mu_assert("averages should be equivalent",
        compare_double(
            hdr_dbl_mean(histogram) * 512,
            hdr_dbl_mean(scaled_histogram),
            hdr_dbl_mean(scaled_histogram) * 0.000001));

    mu_assert("99%'iles should be equivalent",
        compare_double(
            hdr_dbl_highest_equivalent_value(scaled_histogram, hdr_dbl_value_at_percentile(histogram, 99.0)) * 512.0,
            hdr_dbl_highest_equivalent_value(scaled_histogram, hdr_dbl_value_at_percentile(scaled_histogram, 99.0)),
            hdr_dbl_highest_equivalent_value(
                scaled_histogram, hdr_dbl_value_at_percentile(scaled_histogram, 99.0)) * 0.000001));

    mu_assert("Max should be equivalent",
        compare_double(
            hdr_dbl_highest_equivalent_value(scaled_histogram, hdr_dbl_max(histogram) * 512),
            hdr_dbl_max(scaled_histogram),
            hdr_dbl_max(scaled_histogram) * 0.000001));

    mu_assert("total count should be the same",
        compare_int64(
            post_corrected_histogram->values.total_count,
            post_corrected_scaled_histogram->values.total_count));

    mu_assert("99%'iles should be equivalent",
        compare_double(
            hdr_dbl_lowest_equivalent_value(
                post_corrected_histogram, hdr_dbl_value_at_percentile(post_corrected_histogram, 99.0)) * 512.0,
            hdr_dbl_lowest_equivalent_value(
                post_corrected_scaled_histogram, hdr_dbl_value_at_percentile(post_corrected_scaled_histogram, 99.0)),
            hdr_dbl_lowest_equivalent_value(
                post_corrected_scaled_histogram,
                hdr_dbl_value_at_percentile(post_corrected_scaled_histogram, 99.0)) * 0.000001));

    mu_assert("Max should be equivalent",
        compare_double(
            hdr_dbl_highest_equivalent_value(
                post_corrected_scaled_histogram, hdr_dbl_max(post_corrected_histogram)) * 512,
            hdr_dbl_max(post_corrected_scaled_histogram),
            hdr_dbl_max(post_corrected_scaled_histogram)));

    return 0;
}

char* test_get_total_count()
{
    load_histograms();
    mu_assert("Total count is 10,001", compare_int64(10001, raw_histogram->values.total_count));
    mu_assert("Total count is 20,000", compare_int64(20000, histogram->values.total_count));

    return 0;
}

char* test_get_max_value()
{
    load_histograms();
    mu_assert(
        "Max value is 100000000",
        hdr_dbl_values_are_equivalent(histogram, INT64_C(100) * 1000 * 1000, hdr_dbl_max(histogram)));

    return 0;
}

char* test_get_min_value()
{
    load_histograms();
    mu_assert(
        "Min value is 1000",
        hdr_dbl_values_are_equivalent(histogram, INT64_C(1000), hdr_dbl_min(histogram)));

    return 0;
}

char* test_get_mean()
{
    double expected_raw_mean = ((10000.0 * 1000) + (1.0 * 100000000))/10001; // direct avg. of raw results
    mu_assert(
        "Should calculate raw mean",
        compare_double(expected_raw_mean, hdr_dbl_mean(raw_histogram), expected_raw_mean * 0.001));

    double expected_mean = (1000.0 + 50000000.0)/2; // avg. 1 msec for half the time, and 50 sec for other half
    mu_assert(
        "Should calculate mean",
        compare_double(expected_mean, hdr_dbl_mean(histogram), expected_mean * 0.001));

    return 0;
}

char* test_get_raw_stddev()
{
    double expected_raw_mean = ((10000.0 * 1000) + (1.0 * 100000000))/10001; // direct avg. of raw results
    double expected_raw_std_dev =
        sqrt(((10000.0 * pow((1000.0 - expected_raw_mean), 2)) + pow((100000000.0 - expected_raw_mean), 2)) / 10001);

    mu_assert(
        "Raw standard deviation",
        compare_double(expected_raw_std_dev, hdr_dbl_stddev(raw_histogram), expected_raw_std_dev * 0.001));

    return 0;
}

char* test_get_stddev()
{
    double expected_mean = (1000.0 + 50000000.0)/2; // avg. 1 msec for half the time, and 50 sec for other half
    double expected_stddev_sum = 10000 * pow((1000.0 - expected_mean), 2);
    for (long value = 10000; value <= 100000000; value += 10000) {
        expected_stddev_sum += pow((value - expected_mean), 2);
    }
    double expected_stddev = sqrt(expected_stddev_sum / 20000);

    mu_assert(
        "Raw standard deviation",
        compare_double(expected_stddev, hdr_dbl_stddev(histogram), expected_stddev * 0.001));

    return 0;
}

char*test_get_raw_value_at_percentile()
{
    mu_assert(
        "raw 30%'ile is 1 msec +/- 0.1%",
        compare_double(1000.0, hdr_dbl_value_at_percentile(raw_histogram, 30.0), 1000.0 * 0.001));
    mu_assert(
        "raw 99%'ile is 1 msec +/- 0.1%",
        compare_double(1000.0, hdr_dbl_value_at_percentile(raw_histogram, 99.0), 1000.0 * 0.001));
    mu_assert(
        "raw 99.99%'ile is 1 msec +/- 0.1%",
        compare_double(1000.0, hdr_dbl_value_at_percentile(raw_histogram, 99.99), 1000.0 * 0.001));
    mu_assert(
        "raw 99.999%'ile is 100 sec +/- 0.1%",
        compare_double(100000000.0, hdr_dbl_value_at_percentile(raw_histogram, 99.999), 100000000.0 * 0.001));
    mu_assert(
        "raw 100%'ile is 100 sec +/- 0.1%",
        compare_double(100000000.0, hdr_dbl_value_at_percentile(raw_histogram, 100.0), 100000000.0 * 0.001));

    return 0;
}

char* test_get_value_at_percentile()
{
    mu_assert(
        "30%'ile is 1 msec +/- 0.1%",
        compare_double(1000.0, hdr_dbl_value_at_percentile(histogram, 30.0), 1000.0 * 0.001));
    mu_assert(
        "50%'ile is 1 msec +/- 0.1%",
        compare_double(1000.0, hdr_dbl_value_at_percentile(histogram, 50.0), 1000.0 * 0.001));
    mu_assert(
        "75%'ile is 50 sec +/- 0.1%",
        compare_double(50000000.0, hdr_dbl_value_at_percentile(histogram, 75.0), 50000000.0 * 0.001));
    mu_assert(
        "90%'ile is 80 sec +/- 0.1%",
        compare_double(80000000.0, hdr_dbl_value_at_percentile(histogram, 90.0), 80000000.0 * 0.001));
    mu_assert(
        "99%'ile is 98 sec +/- 0.1%",
        compare_double(98000000.0, hdr_dbl_value_at_percentile(histogram, 99.0), 98000000.0 * 0.001));
    mu_assert(
        "99.999%'ile is 100 sec +/- 0.1%",
        compare_double(100000000.0, hdr_dbl_value_at_percentile(histogram, 99.999), 100000000.0 * 0.001));
    mu_assert(
        "100%'ile is 100 sec +/- 0.1%",
        compare_double(100000000.0, hdr_dbl_value_at_percentile(histogram, 100.0), 100000000.0 * 0.001));

    return 0;
}

/*
    @Test
    public void testLinearBucketValues() throws Exception {
        int index = 0;
        // Note that using linear buckets should work "as expected" as long as the number of linear buckets
        // is lower than the resolution level determined by largestValueWithSingleUnitResolution
        // (2000 in this case). Above that count, some of the linear buckets can end up rounded up in size
        // (to the nearest local resolution unit level), which can result in a smaller number of buckets that
        // expected covering the range.

        // Iterate raw data using linear buckets of 100 msec each.
        for (DoubleHistogramIterationValue v : rawHistogram.linearBucketValues(100000)) {
            long countAddedInThisBucket = v.getCountAddedInThisIterationStep();
            if (index == 0) {
                Assert.assertEquals("Raw Linear 100 msec bucket # 0 added a count of 10000",
                        10000, countAddedInThisBucket);
            } else if (index == 999) {
                Assert.assertEquals("Raw Linear 100 msec bucket # 999 added a count of 1",
                        1, countAddedInThisBucket);
            } else {
                Assert.assertEquals("Raw Linear 100 msec bucket # " + index + " added a count of 0",
                        0 , countAddedInThisBucket);
            }
            index++;
        }
        Assert.assertEquals(1000, index);
        */

char* test_get_value_linear()
{

}

/*

        index = 0;
        long totalAddedCounts = 0;
        // Iterate data using linear buckets of 10 msec each.
        for (DoubleHistogramIterationValue v : histogram.linearBucketValues(10000)) {
            long countAddedInThisBucket = v.getCountAddedInThisIterationStep();
            if (index == 0) {
                Assert.assertEquals("Linear 1 sec bucket # 0 [" +
                        v.getValueIteratedFrom() + ".." + v.getValueIteratedTo() +
                        "] added a count of 10001",
                        10001, countAddedInThisBucket);
            }
            // Because value resolution is low enough (3 digits) that multiple linear buckets will end up
            // residing in a single value-equivalent range, some linear buckets will have counts of 2 or
            // more, and some will have 0 (when the first bucket in the equivalent range was the one that
            // got the total count bump).
            // However, we can still verify the sum of counts added in all the buckets...
            totalAddedCounts += v.getCountAddedInThisIterationStep();
            index++;
        }
        Assert.assertEquals("There should be 10000 linear buckets of size 10000 usec between 0 and 100 sec.",
                10000, index);
        Assert.assertEquals("Total added counts should be 20000", 20000, totalAddedCounts);

        index = 0;
        totalAddedCounts = 0;
        // Iterate data using linear buckets of 1 msec each.
        for (DoubleHistogramIterationValue v : histogram.linearBucketValues(1000)) {
            long countAddedInThisBucket = v.getCountAddedInThisIterationStep();
            if (index == 0) {
                Assert.assertEquals("Linear 1 sec bucket # 0 [" +
                        v.getValueIteratedFrom() + ".." + v.getValueIteratedTo() +
                        "] added a count of 10000",
                        10000, countAddedInThisBucket);
            }
            // Because value resolution is low enough (3 digits) that multiple linear buckets will end up
            // residing in a single value-equivalent range, some linear buckets will have counts of 2 or
            // more, and some will have 0 (when the first bucket in the equivalent range was the one that
            // got the total count bump).
            // However, we can still verify the sum of counts added in all the buckets...
            totalAddedCounts += v.getCountAddedInThisIterationStep();
            index++;
        }
        // You may ask "why 100007 and not 100000?" for the value below? The answer is that at this fine
        // a linear stepping resolution, the final populated sub-bucket (at 100 seconds with 3 decimal
        // point resolution) is larger than our liner stepping, and holds more than one linear 1 msec
        // step in it.
        // Since we only know we're done with linear iteration when the next iteration step will step
        // out of the last populated bucket, there is not way to tell if the iteration should stop at
        // 100000 or 100007 steps. The proper thing to do is to run to the end of the sub-bucket quanta...
        Assert.assertEquals("There should be 100007 linear buckets of size 1000 usec between 0 and 100 sec.",
                100007, index);
        Assert.assertEquals("Total added counts should be 20000", 20000, totalAddedCounts);
    }
 */

static struct mu_result all_tests() {
    mu_run_test(test_scaling_equivalence);
    mu_run_test(test_get_total_count);
    mu_run_test(test_get_max_value);
    mu_run_test(test_get_min_value);
    mu_run_test(test_get_mean);
    mu_run_test(test_get_raw_stddev);
    mu_run_test(test_get_stddev);
    mu_run_test(test_get_raw_value_at_percentile);
    mu_run_test(test_get_value_at_percentile);

    mu_ok;
}

int hdr_dbl_histogram_data_access_run_tests() {
    struct mu_result result = all_tests();

    if (result.message != 0) {
        printf("hdr_dbl_histogram_data_access_test.%s(): %s\n", result.test, result.message);
    }
    else {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return (int) result.message;
}


int main(int argc, char **argv) {
    return hdr_dbl_histogram_data_access_run_tests();
}