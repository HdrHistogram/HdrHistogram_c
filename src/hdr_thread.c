/**
* hdr_thread.c
* Written by Philip Orwig and released to the public domain,
* as explained at http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <stdlib.h>
#include "hdr_thread.h"

struct hdr_mutex* hdr_mutex_alloc(void)
{
    return malloc(sizeof(hdr_mutex));
}

void hdr_mutex_free(struct hdr_mutex* mutex)
{
    free(mutex);
}

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

int hdr_mutex_init(struct hdr_mutex* mutex)
{
	InitializeCriticalSection((CRITICAL_SECTION*)(mutex->_critical_section));
	return 0;
}

void hdr_mutex_destroy(struct hdr_mutex* mutex)
{
	DeleteCriticalSection((CRITICAL_SECTION*)(mutex->_critical_section));
}

void hdr_mutex_lock(struct hdr_mutex* mutex)
{
	EnterCriticalSection((CRITICAL_SECTION*)(mutex->_critical_section));
}

void hdr_mutex_unlock(struct hdr_mutex* mutex)
{
	LeaveCriticalSection((CRITICAL_SECTION*)(mutex->_critical_section));
}

#else
#include <pthread.h>

int hdr_mutex_init(struct hdr_mutex* mutex)
{
	return pthread_mutex_init(&mutex->_mutex, NULL);
}

void hdr_mutex_destroy(struct hdr_mutex* mutex)
{
	pthread_mutex_destroy(&mutex->_mutex);
}

void hdr_mutex_lock(struct hdr_mutex* mutex)
{
	pthread_mutex_lock(&mutex->_mutex);
}

void hdr_mutex_unlock(struct hdr_mutex* mutex)
{
	pthread_mutex_unlock(&mutex->_mutex);
}

#endif
