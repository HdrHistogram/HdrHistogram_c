/**
 * hdr_interval_recorder.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_INTERVAL_RECORDER_H
#define HDR_INTERVAL_RECORDER_H 1

#include <hdr_writer_reader_phaser.h>

struct hdr_interval_recorder
{
    void* active;
    void* inactive;
    struct hdr_writer_reader_phaser phaser;
} __attribute__((aligned (8)));

int hdr_interval_recorder_init(struct hdr_interval_recorder* r);

void hdr_interval_recorder_destroy(struct hdr_interval_recorder* r);

void hdr_interval_recorder_update(
    struct hdr_interval_recorder* r, 
    void(*update_action)(void*, void*), 
    void* arg);

void* hdr_interval_recorder_sample(struct hdr_interval_recorder* r);

#endif