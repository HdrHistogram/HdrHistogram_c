/**
 * hdr_interval_recorder.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "hdr_atomic.h"
#include "hdr_interval_recorder.h"

int hdr_interval_recorder_init(struct hdr_interval_recorder* r)
{
    return hdr_writer_reader_phaser_init(&r->phaser);
}

int hdr_interval_recorder_init_all(
    struct hdr_interval_recorder* r,
    int64_t lowest_trackable_value,
    int64_t highest_trackable_value,
    int significant_figures)
{
    struct hdr_histogram** active = (struct hdr_histogram**) &r->active;
    struct hdr_histogram** inactive = (struct hdr_histogram**) &r->inactive;

    int result = hdr_writer_reader_phaser_init(&r->phaser);
    result = result == 0
        ? hdr_init(lowest_trackable_value, highest_trackable_value, significant_figures, active)
        : result;

    result = result == 0
        ? hdr_init(lowest_trackable_value, highest_trackable_value, significant_figures, inactive)
        : result;

    return result;
}

void hdr_interval_recorder_destroy(struct hdr_interval_recorder* r)
{
    hdr_writer_reader_phaser_destroy(&r->phaser);
}

void hdr_interval_recorder_update(
    struct hdr_interval_recorder* r, 
    void(*update_action)(void*, void*), 
    void* arg)
{
    int64_t val = hdr_phaser_writer_enter(&r->phaser);

    void* active = hdr_atomic_load_pointer(&r->active);

    update_action(active, arg);

    hdr_phaser_writer_exit(&r->phaser, val);
}

void* hdr_interval_recorder_sample(struct hdr_interval_recorder* r)
{
    void* temp;

    hdr_phaser_reader_lock(&r->phaser);

    temp = r->inactive;

    // volatile read
    r->inactive = hdr_atomic_load_pointer(&r->active);

    // volatile write
    hdr_atomic_store_pointer(&r->active, temp);

    hdr_phaser_flip_phase(&r->phaser, 0);

    hdr_phaser_reader_unlock(&r->phaser);

    return r->inactive;
}

static void update_value(void* data, void* arg)
{
    struct hdr_histogram* h = data;
    int64_t* params = arg;
    params[1] = hdr_record_value(h, params[0]);
}

int64_t hdr_interval_recorder_record_value(
    struct hdr_interval_recorder* r,
    int64_t value
)
{
    int64_t params[2];
    params[0] = value;
    params[1] = 0;

    hdr_interval_recorder_update(r, update_value, &params[0]);
    return params[1];
}

static void update_values(void* data, void* arg)
{
    struct hdr_histogram* h = data;
    int64_t* params = arg;
    params[2] = hdr_record_values(h, params[0], params[1]);
}

int64_t hdr_interval_recorder_record_values(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t count
)
{
    int64_t params[3];
    params[0] = value;
    params[1] = count;
    params[2] = 0;

    hdr_interval_recorder_update(r, update_values, &params[0]);
    return params[2];
}

static void update_corrected_value(void* data, void* arg)
{
    struct hdr_histogram* h = data;
    int64_t* params = arg;
    params[2] = hdr_record_corrected_value(h, params[0], params[1]);
}

int64_t hdr_interval_recorder_record_corrected_value(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t expected_interval
)
{
    int64_t params[3];
    params[0] = value;
    params[1] = expected_interval;
    params[2] = 0;

    hdr_interval_recorder_update(r, update_corrected_value, &params[0]);
    return params[2];
}

static void update_corrected_values(void* data, void* arg)
{
    struct hdr_histogram* h = data;
    int64_t* params = arg;
    params[3] = hdr_record_corrected_values(h, params[0], params[1], params[2]);
}

int64_t hdr_interval_recorder_record_corrected_values(
    struct hdr_interval_recorder* r,
    int64_t value,
    int64_t count,
    int64_t expected_interval
)
{
    int64_t params[4];
    params[0] = value;
    params[1] = count;
    params[2] = expected_interval;
    params[3] = 0;

    hdr_interval_recorder_update(r, update_corrected_values, &params[0]);
    return params[3];

}
