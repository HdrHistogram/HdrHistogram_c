/**
* hdr_time.h
* Written by Michael Barker and Philip Orwig and released to the public domain,
* as explained at http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <math.h>
#include <limits.h>

#include <hdr/hdr_time.h>

#if defined(_WIN32) || defined(_WIN64)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

static int s_clockPeriodSet = 0;
static double s_clockPeriod = 1.0;

void hdr_gettime(hdr_timespec* t)
{
    LARGE_INTEGER num;
    /* if this is distasteful, we can add in an hdr_time_init() */
    if (!s_clockPeriodSet)
    {
        QueryPerformanceFrequency(&num);
        s_clockPeriod = 1.0 / (double) num.QuadPart;
        s_clockPeriodSet = 1;
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


void hdr_gettime(hdr_timespec* ts)
{
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
}


void hdr_getnow(hdr_timespec* ts)
{
    hdr_gettime(ts);
}

#elif defined(__linux__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)


void hdr_gettime(hdr_timespec* t)
{
    clock_gettime(CLOCK_MONOTONIC, (struct timespec*)t);
}

void hdr_getnow(hdr_timespec* t)
{
    clock_gettime(CLOCK_REALTIME, (struct timespec*)t);
}

#else

#warning "Platform not supported\n"

#endif

double hdr_timespec_as_double(const hdr_timespec* t)
{
    double d = t->tv_sec;
    return d + (t->tv_nsec / 1000000000.0);
}

void hdr_timespec_from_double(hdr_timespec* t, double value)
{
    /* tv_sec is a long on Windows; 2^(bits-1) is exact as a double and one past its max */
    const double limit = ldexp(1.0, (int) (sizeof(long) * CHAR_BIT) - 1);
    double seconds;
    long milliseconds;

    if (!isfinite(value))
    {
        t->tv_sec = 0;
        t->tv_nsec = 0;
        return;
    }

    /* floor, not trunc: tv_nsec must be in [0, 1e9), so the remainder cannot be negative */
    seconds = floor(value);
    milliseconds = (long) round((value - seconds) * 1000);
    if (milliseconds == 1000)
    {
        /* rounded up to a whole second; carry rather than emit tv_nsec == 1e9 */
        seconds += 1.0;
        milliseconds = 0;
    }

    /* converting an out-of-range double to an integer is UB */
    if (seconds >= limit || seconds < -limit)
    {
        t->tv_sec = 0;
        t->tv_nsec = 0;
        return;
    }

    t->tv_sec = (long) seconds;
    t->tv_nsec = milliseconds * 1000000;
}
