#include <hdr/hdr_histogram.h>
#include <hdr/hdr_histogram_log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char filename[256];
  hdr_timespec timestamp, interval;
  struct hdr_histogram *h = NULL;
  struct hdr_log_reader reader;
  int rc = 0;

  sprintf(filename, "/tmp/libfuzzer.%d", getpid());

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    return 0;
  }
  fwrite(data, size, 1, fp);
  fclose(fp);

  // open FP to the log file
  fp = fopen(filename, "r");
  if (hdr_log_reader_init(&reader)) {
    return 0;
  }

  rc = hdr_log_read_header(&reader, fp);
  if (rc) {
    fclose(fp);
    unlink(filename);
    return 0;
  }

  // Output to /dev/null
  FILE *fp_dev_null = fopen("/dev/null", "w");

  rc = hdr_log_read(&reader, fp, &h, &timestamp, &interval);

  if (0 == rc) {
    // Call functions used by NodeJS
    hdr_min(h);
    hdr_max(h);
    hdr_mean(h);
    hdr_stddev(h);
    hdr_value_at_percentile(h, 50.0);
    hdr_get_memory_size(h);

    hdr_percentiles_print(h, fp_dev_null, 5, 1.0, CLASSIC);
    hdr_close(h);
  }

  fclose(fp_dev_null);
  fclose(fp);

  unlink(filename);

  return 0;
}
