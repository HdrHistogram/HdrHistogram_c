/**
 * hdr_time.h
 * Written by Philip Orwig and released to the public domain,
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

typedef struct timespec hdr_timespec;

#endif

#if defined(_MSC_VER)
void hdr_gettime(hdr_timespec* t);
#else
inline void hdr_gettime(hdr_timespec* t);
#endif

#endif
