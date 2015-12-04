/**
 * hdr_writer_reader_phaser.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#if defined(_MSC_VER)
#include <WinSock2.h>
#else
#include <unistd.h>
#include <sched.h>
#endif

#include "hdr_atomic.h"
#include "hdr_thread.h"

#include "hdr_writer_reader_phaser.h"

static int64_t _hdr_phaser_get_epoch(int64_t* field)
{
    return hdr_atomic_load_64(field);
}

static void _hdr_phaser_set_epoch(int64_t* field, int64_t val)
{
    hdr_atomic_store_64(field, val);
}

static int64_t _hdr_phaser_reset_epoch(int64_t* field, int64_t initial_value)
{
    return hdr_atomic_exchange_64(field, initial_value);
}

int hdr_writer_reader_phaser_init(struct hdr_writer_reader_phaser* p)
{
    if (NULL == p)
    {
        return EINVAL;
    }

    p->start_epoch = 0;
    p->even_end_epoch = 0;
    p->odd_end_epoch = INT64_MIN;
	p->reader_mutex = hdr_mutex_alloc();

    if (!p->reader_mutex)
    {
        return ENOMEM;
    }

    int rc = hdr_mutex_init(p->reader_mutex);
    if (0 != rc)
    {
        return rc;
    }

    // TODO: Should I fence here.

    return 0;
}

void hdr_writer_reader_phaser_destory(struct hdr_writer_reader_phaser* p)
{
    hdr_mutex_destroy(p->reader_mutex);
}

int64_t hdr_phaser_writer_enter(struct hdr_writer_reader_phaser* p)
{
    return hdr_atomic_fetch_add_64(&p->start_epoch, 1) + 1;
}

void hdr_phaser_writer_exit(
    struct hdr_writer_reader_phaser* p, int64_t critical_value_at_enter)
{
    int64_t* end_epoch = 
        (critical_value_at_enter < 0) ? &p->odd_end_epoch : &p->even_end_epoch;
    hdr_atomic_fetch_add_64(end_epoch, 1);
}

void hdr_phaser_reader_lock(struct hdr_writer_reader_phaser* p)
{
    hdr_mutex_lock(p->reader_mutex);
}

void hdr_phaser_reader_unlock(struct hdr_writer_reader_phaser* p)
{
    hdr_mutex_unlock(p->reader_mutex);
}

void hdr_phaser_flip_phase(
    struct hdr_writer_reader_phaser* p, int64_t sleep_time_ns)
{
    // TODO: is_held_by_current_thread

    int64_t start_epoch = _hdr_phaser_get_epoch(&p->start_epoch);

    bool next_phase_is_even = (start_epoch < 0);

    // Clear currently used phase end epoch.
    int64_t initial_start_value;
    if (next_phase_is_even)
    {
        initial_start_value = 0;
        _hdr_phaser_set_epoch(&p->even_end_epoch, initial_start_value);
    }
    else
    {
        initial_start_value = INT64_MIN;
        _hdr_phaser_set_epoch(&p->odd_end_epoch, initial_start_value);
    }

    // Reset start value, indicating new phase.
    int64_t start_value_at_flip = 
        _hdr_phaser_reset_epoch(&p->start_epoch, initial_start_value);

    bool caught_up = false;
    do
    {
        int64_t* end_epoch = 
            next_phase_is_even ? &p->odd_end_epoch : &p->even_end_epoch;

        caught_up = _hdr_phaser_get_epoch(end_epoch) == start_value_at_flip;

        if (!caught_up)
        {
            if (sleep_time_ns == 0)
            {
#if defined(_MSC_VER)
				Sleep(0);
#else
                sched_yield();
#endif
            }
            else
            {
#if defined(_MSC_VER)
				struct timeval tv;

				tv.tv_sec = (long)sleep_time_ns / 1000000;
				tv.tv_usec = sleep_time_ns % 1000000;				
				select(0, NULL, NULL, NULL, &tv);
#else
                usleep(sleep_time_ns / 1000);
#endif
            }
        }
    }
    while (!caught_up);
}
