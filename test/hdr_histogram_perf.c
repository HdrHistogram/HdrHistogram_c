/**
 * hdr_histogram_perf.c
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <hdr_histogram.h>
#include <locale.h>

#include "hdr_time.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#endif


static hdr_timespec diff(hdr_timespec start, hdr_timespec end)
{
    hdr_timespec temp;
    if ((end.tv_nsec-start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

int main()
{
    struct hdr_histogram* histogram;
    int64_t max_value = INT64_C(24) * 60 * 60 * 1000000;
    int64_t min_value = 1;
    int result = -1;

    result = hdr_init(min_value, max_value, 4, &histogram);
    if (result != 0)
    {
        fprintf(stderr, "Failed to allocate histogram: %d\n", result);
        return -1;
    }

    hdr_timespec t0;
    hdr_timespec t1;
    setlocale(LC_NUMERIC, "");
    int64_t iterations = 400000000;

    int i;
    for (i = 0; i < 100; i++)
    {
        int64_t j;
        hdr_gettime(&t0);
        for (j = 1; j < iterations; j++)
        {
            hdr_record_value(histogram, j);
        }
        hdr_gettime(&t1);

        hdr_timespec taken = diff(t0, t1);
        double time_taken = taken.tv_sec + taken.tv_nsec / 1000000000.0;
        double ops_sec = (iterations - 1) / time_taken;

#if defined(_MSC_VER)
        wchar_t unformatted[64];
        _snwprintf_s(unformatted, sizeof(unformatted), sizeof(unformatted) - 1, L"%.2f", ops_sec);
        wchar_t formatted[64];
        int ret = GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, unformatted, NULL, formatted, sizeof(formatted));
        wprintf_s(L"%s - %d, ops/sec: %s\n", L"Iteration", i + 1, formatted);
#else
        printf("%s - %d, ops/sec: %'.2f\n", "Iteration", i + 1, ops_sec);
#endif
    }

    return 0;
}
