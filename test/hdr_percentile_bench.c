#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_time.h>

static hdr_timespec diff(hdr_timespec s, hdr_timespec e) {
    hdr_timespec t;
    if (e.tv_nsec < s.tv_nsec) { t.tv_sec = e.tv_sec-s.tv_sec-1; t.tv_nsec = 1000000000+e.tv_nsec-s.tv_nsec; }
    else { t.tv_sec = e.tv_sec-s.tv_sec; t.tv_nsec = e.tv_nsec-s.tv_nsec; }
    return t;
}

int main(void) {
    struct hdr_histogram* h;
    hdr_init(1, INT64_C(3600000000), 3, &h);

    /* Spread non-zero entries across the full bucket range so the percentile
       scan actually walks past the prologue. Fibonacci-hash spread (constant
       2654435761) gives an even distribution over a coprime modulus. */
    for (int64_t v = 1; v <= 1000000; v++) {
        int64_t value = (int64_t)(((uint64_t)v * 2654435761u) % 1000000000u) + 1;
        hdr_record_value(h, value);
    }

    const double percentiles[] = {50.0, 75.0, 90.0, 95.0, 99.0, 99.9, 99.99};
    const int n_percentiles = (int)(sizeof(percentiles) / sizeof(percentiles[0]));
    const int64_t iters = 1000000;
    const int warmup_runs = 3;
    const int n_runs = 20;

    double best_secs = 1e18, total_secs = 0;
    int64_t sink_total = 0;
    for (int run = 0; run < warmup_runs + n_runs; run++) {
        hdr_timespec t0, t1;
        hdr_gettime(&t0);
        volatile int64_t sink = 0;
        for (int64_t i = 0; i < iters; i++)
            sink += hdr_value_at_percentile(h, percentiles[i % n_percentiles]);
        hdr_gettime(&t1);
        hdr_timespec taken = diff(t0, t1);
        double secs = taken.tv_sec + taken.tv_nsec / 1e9;
        if (run >= warmup_runs) {
            if (secs < best_secs) best_secs = secs;
            total_secs += secs;
            sink_total += sink;
        }
    }
    double best_qps = iters / best_secs / 1e6;
    double mean_qps = iters / (total_secs / n_runs) / 1e6;
    printf("best: %.2f M queries/sec  mean: %.2f M queries/sec  (sink=%lld)\n",
           best_qps, mean_qps, (long long)sink_total);

    hdr_close(h);
    return 0;
}
