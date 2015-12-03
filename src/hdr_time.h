/**
 * hdr_time.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_TIME_H__
#define HDR_TIME_H__

#include <math.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

typedef struct hdr_timespec
{
    long tv_sec;
    long tv_nsec;
} hdr_timespec;

#else

typedef timespec hdr_timespec;

#endif

static void hdr_gettime(struct hdr_timespec* t);

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>


static bool s_clockPeriodSet = false;
static double s_clockPeriod = 1.0;

static void hdr_gettime(struct hdr_timespec* t)
{
    LARGE_INTEGER num;
    /* if this is distasteful, we can add in an hdr_time_init() */
    if (!s_clockPeriodSet)
    {
        QueryPerformanceFrequency(&num);
        s_clockPeriod = 1.0 / (double) num.QuadPart;
        s_clockPeriodSet = true;
    }

    QueryPerformanceCounter(&num);
    double seconds = num.QuadPart * s_clockPeriod;
    double integral;
    double remainder = modf(seconds, &integral);

    t->tv_sec  = (long) integral;
    t->tv_nsec = (long) (remainder * 1000000000);
}

#elif defined(__APPLE__)
#include <mach/clock.h>
#include <mach/mach.h>

typedef timespec hdr_timespec;

static void hdr_gettime(struct hdr_timespec* ts)
{
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
}

#elif defined(__linux__)

typedef timespec hdr_timespec;

static void hdr_gettime(struct hdr_timespec* t)
{
    clock_gettime(CLOCK_MONOTONIC, t);
}

#else

#warning "Platform not supported\n"

#endif

#endif
