/**
 * hdr_interval_recorder.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_INTERVAL_RECORDER_H
#define HDR_INTERVAL_RECORDER_H 1

#include "hdr_writer_reader_phaser.h"
#include "hdr_histogram.h"

HDR_ALIGN_PREFIX(8)
struct hdr_interval_recorder
{
    void* active;
    void* inactive;
    struct hdr_writer_reader_phaser phaser;
} 
HDR_ALIGN_SUFFIX(8);

#ifdef __cplusplus
extern "C" {
#endif

int hdr_interval_recorder_init(struct hdr_interval_recorder* r);

int hdr_interval_recorder_init_all(
    struct hdr_interval_recorder* r,
    int64_t lowest_trackable_value,
    int64_t highest_trackable_value,
    int significant_figures);

void hdr_interval_recorder_destroy(struct hdr_interval_recorder* r);

void hdr_interval_recorder_update(
    struct hdr_interval_recorder* r, 
    void(*update_action)(void*, void*), 
    void* arg);

int64_t hdr_interval_recorder_record_value(
    struct hdr_interval_recorder* r,
    int64_t value
);

int64_t hdr_interval_recorder_record_values(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t count
);

int64_t hdr_interval_recorder_record_corrected_value(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t expected_interval
);

int64_t hdr_interval_recorder_record_corrected_values(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t count,
    int64_t expected_interval
);

void* hdr_interval_recorder_sample(struct hdr_interval_recorder* r);

#ifdef __cplusplus
}
#endif

#endif
