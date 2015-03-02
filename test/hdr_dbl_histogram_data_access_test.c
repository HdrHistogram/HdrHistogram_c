#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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

/*
    static {
        histogram = new DoubleHistogram(highestTrackableValue, numberOfSignificantValueDigits);
        scaledHistogram = new DoubleHistogram(highestTrackableValue / 2 , numberOfSignificantValueDigits);
        rawHistogram = new DoubleHistogram(highestTrackableValue, numberOfSignificantValueDigits);
        scaledRawHistogram = new DoubleHistogram(highestTrackableValue / 2, numberOfSignificantValueDigits);
        // Log hypothetical scenario: 100 seconds of "perfect" 1msec results, sampled
        // 100 times per second (10,000 results), followed by a 100 second pause with
        // a single (100 second) recorded result. Recording is done indicating an expected
        // interval between samples of 10 msec:
        for (int i = 0; i < 10000; i++) {
            histogram.recordValueWithExpectedInterval(1000 , 10000);
            scaledHistogram.recordValueWithExpectedInterval(1000 * 512, 10000 * 512);
            rawHistogram.recordValue(1000);
            scaledRawHistogram.recordValue(1000 * 512);
        }
        histogram.recordValueWithExpectedInterval(100000000L, 10000);
        scaledHistogram.recordValueWithExpectedInterval(100000000L * 512, 10000 * 512);
        rawHistogram.recordValue(100000000L);
        scaledRawHistogram.recordValue(100000000L * 512);

        postCorrectedHistogram = rawHistogram.copyCorrectedForCoordinatedOmission(10000);
        postCorrectedScaledHistogram = scaledRawHistogram.copyCorrectedForCoordinatedOmission(10000 * 512);
    }
 */
void load_histograms()
{
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
    hdr_dbl_record_corrected_value(histogram, INT64_C(100000000L), 1000);
    hdr_dbl_record_corrected_value(scaled_histogram, INT64_C(100000000L) * 512, 1000 * 512);

    hdr_dbl_add_while_correcting_for_coordinated_omission(&post_corrected_histogram, raw_histogram, 10000);
    hdr_dbl_add_while_correcting_for_coordinated_omission(&post_corrected_scaled_histogram, scaled_raw_histogram, 512 * 10000);
}

char* test_scaling_equivalence()
{
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
                    hdr_dbl_highest_equivalent_value(scaled_histogram, hdr_dbl_value_at_percentile(scaled_histogram, 99.0)) * 0.000001));

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
                    hdr_dbl_lowest_equivalent_value(post_corrected_histogram, hdr_dbl_value_at_percentile(post_corrected_histogram, 99.0)) * 512.0,
                    hdr_dbl_lowest_equivalent_value(post_corrected_scaled_histogram, hdr_dbl_value_at_percentile(post_corrected_scaled_histogram, 99.0)),
                    hdr_dbl_lowest_equivalent_value(post_corrected_scaled_histogram, hdr_dbl_value_at_percentile(post_corrected_scaled_histogram, 99.0)) * 0.000001));

    mu_assert("Max should be equivalent",
            compare_double(
                    hdr_dbl_highest_equivalent_value(post_corrected_scaled_histogram, hdr_dbl_max(post_corrected_histogram)) * 512,
                    hdr_dbl_max(post_corrected_scaled_histogram),
                    hdr_dbl_max(post_corrected_scaled_histogram)));

    return 0;
}


static struct mu_result all_tests()
{
    mu_run_test(test_scaling_equivalence);

    mu_ok;
}

int hdr_dbl_histogram_data_access_run_tests()
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


int main(int argc, char** argv)
{
    return hdr_dbl_histogram_data_access_run_tests();
}