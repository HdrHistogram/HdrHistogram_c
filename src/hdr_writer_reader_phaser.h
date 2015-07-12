/**
 * hdr_writer_reader_phaser.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_WRITER_READER_PHASER_H
#define HDR_WRITER_READER_PHASER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

struct hdr_writer_reader_phaser
{
    int64_t start_epoch;
    int64_t even_end_epoch;
    int64_t odd_end_epoch;
    pthread_mutex_t* reader_mutex;
} __attribute__((aligned (8)));

int hdr_writer_reader_phaser_init(struct hdr_writer_reader_phaser* p);

void hdr_writer_reader_phaser_destory(struct hdr_writer_reader_phaser* p);

int64_t hdr_phaser_writer_enter(struct hdr_writer_reader_phaser* p);

void hdr_phaser_writer_exit(
    struct hdr_writer_reader_phaser* p, int64_t critical_value_at_enter);

void hdr_phaser_reader_lock(struct hdr_writer_reader_phaser* p);

void hdr_phaser_reader_unlock(struct hdr_writer_reader_phaser* p);

void hdr_phaser_flip_phase(
    struct hdr_writer_reader_phaser* p, int64_t sleep_time_ns);

#endif