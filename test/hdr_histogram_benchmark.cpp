#include <benchmark/benchmark.h>
#include <hdr_histogram.h>
#include <math.h>
#include <random>

#ifdef _WIN32
#pragma comment(lib, "Shlwapi.lib")
#ifdef _DEBUG
#pragma comment(lib, "benchmarkd.lib")
#else
#pragma comment(lib, "benchmark.lib")
#endif
#endif

int64_t min_value = 1;
int64_t min_precision = 3;
int64_t max_precision = 4;
int64_t min_time_unit = 1000;
int64_t max_time_unit = 1000000;
int64_t step_time_unit = 1000;
int64_t generated_datapoints = 10000000;

static void generate_arguments_pairs(benchmark::internal::Benchmark *b) {
  for (int64_t precision = min_precision; precision <= max_precision;
       precision++) {
    for (int64_t time_unit = min_time_unit; time_unit <= max_time_unit;
         time_unit *= step_time_unit) {
      b = b->ArgPair(precision, INT64_C(24) * 60 * 60 * time_unit);
    }
  }
}

static void BM_hdr_init(benchmark::State &state) {
  const int64_t precision = state.range(0);
  const int64_t max_value = state.range(1);
  for (auto _ : state) {
    struct hdr_histogram *histogram;
    benchmark::DoNotOptimize(
        hdr_init(min_value, max_value, precision, &histogram));
    // read/write barrier
    benchmark::ClobberMemory();
  }
}

static void BM_hdr_record_values(benchmark::State &state) {
  const int64_t precision = state.range(0);
  const int64_t max_value = state.range(1);
  struct hdr_histogram *histogram;
  hdr_init(min_value, max_value, precision, &histogram);
  benchmark::DoNotOptimize(histogram->counts);

  for (auto _ : state) {
    benchmark::DoNotOptimize(hdr_record_values(histogram, 1000000, 1));
    // read/write barrier
    benchmark::ClobberMemory();
  }
}

static void BM_hdr_value_at_percentile(benchmark::State &state) {
  srand(12345);
  const int64_t precision = state.range(0);
  const int64_t max_value = state.range(1);
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 100);
  std::default_random_engine generator;
  // gama distribution shape 1 scale 100000
  std::gamma_distribution<double> latency_gamma_dist(1.0, 100000);
  struct hdr_histogram *histogram;
  hdr_init(min_value, max_value, precision, &histogram);
  for (int64_t i = 1; i < generated_datapoints; i++) {
    int64_t number = int64_t(latency_gamma_dist(generator)) + 1;
    number = number > max_value ? max_value : number;
    hdr_record_value(histogram, number);
  }
  benchmark::DoNotOptimize(histogram->counts);
  for (auto _ : state) {
    state.PauseTiming();
    const double percentile = dist(e2);
    state.ResumeTiming();
    benchmark::DoNotOptimize(hdr_value_at_percentile(histogram, percentile));
    // read/write barrier
    benchmark::ClobberMemory();
  }
}

static void BM_hdr_value_at_percentile_given_array(benchmark::State &state) {
  srand(12345);
  const int64_t precision = state.range(0);
  const int64_t max_value = state.range(1);
  const double percentile_list[4] = {50.0, 95.0, 99.0, 99.9};
  std::default_random_engine generator;
  // gama distribution shape 1 scale 100000
  std::gamma_distribution<double> latency_gamma_dist(1.0, 100000);
  struct hdr_histogram *histogram;
  hdr_init(min_value, max_value, precision, &histogram);
  for (int64_t i = 1; i < generated_datapoints; i++) {
    int64_t number = int64_t(latency_gamma_dist(generator)) + 1;
    number = number > max_value ? max_value : number;
    hdr_record_value(histogram, number);
  }
  benchmark::DoNotOptimize(histogram->counts);
  int64_t items_processed = 0;
  for (auto _ : state) {
    for (auto percentile : percentile_list) {
      benchmark::DoNotOptimize(hdr_value_at_percentile(histogram, percentile));
      // read/write barrier
      benchmark::ClobberMemory();
    }
    items_processed += 4;
  }
  state.SetItemsProcessed(items_processed);
}

static void BM_hdr_value_at_percentiles_given_array(benchmark::State &state) {
  srand(12345);
  const int64_t precision = state.range(0);
  const int64_t max_value = state.range(1);
  const double percentile_list[4] = {50.0, 95.0, 99.0, 99.9};
  std::default_random_engine generator;
  // gama distribution shape 1 scale 100000
  std::gamma_distribution<double> latency_gamma_dist(1.0, 100000);
  struct hdr_histogram *histogram;
  hdr_init(min_value, max_value, precision, &histogram);
  for (int64_t i = 1; i < generated_datapoints; i++) {
    int64_t number = int64_t(latency_gamma_dist(generator)) + 1;
    number = number > max_value ? max_value : number;
    hdr_record_value(histogram, number);
  }
  benchmark::DoNotOptimize(histogram->counts);
  int64_t *values = (int64_t*) malloc(4 * sizeof(int64_t));
  benchmark::DoNotOptimize(values);
  int64_t items_processed = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        hdr_value_at_percentiles(histogram, percentile_list, values, 4));
    // read/write barrier
    benchmark::ClobberMemory();
    items_processed += 4;
  }
  state.SetItemsProcessed(items_processed);
}

// Register the functions as a benchmark
BENCHMARK(BM_hdr_init)->Apply(generate_arguments_pairs);
BENCHMARK(BM_hdr_record_values)->Apply(generate_arguments_pairs);
BENCHMARK(BM_hdr_value_at_percentile)->Apply(generate_arguments_pairs);
BENCHMARK(BM_hdr_value_at_percentile_given_array)
    ->Apply(generate_arguments_pairs);
BENCHMARK(BM_hdr_value_at_percentiles_given_array)
    ->Apply(generate_arguments_pairs);
BENCHMARK_MAIN();