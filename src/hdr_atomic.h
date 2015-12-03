#pragma once

#ifndef HDR_ATOMIC_H__
#define HDR_ATOMIC_H__


#if defined(_MSC_VER)

#include <stdint.h>
#include <intrin.h>

int64_t __inline hdr_atomic_load(int64_t* field)
{ 
	_ReadBarrier();
	return *field;
}

void __inline hdr_atomic_store(int64_t* field, int64_t value)
{
	_WriteBarrier();
	*field = value;
}

int64_t __inline hdr_atomic_exchange(volatile int64_t* field, int64_t initial)
{
	return _InterlockedExchange64(field, initial);
}

int64_t __inline hdr_atomic_fetch_add(volatile int64_t* field, int64_t value)
{
	return _InterlockedExchangeAdd64(field, value);
}

#else
#include <atomic.h>
#define hdr_atomic_load(x) atomic_load(x)
#define hdr_atomic_store(f,v) atomic_store(f,v)
#define hdr_atomic_exchange(f,i) atomic_exchange(f,i)
#define hdr_atomic_fetch_add(field, int64_t value) atomic_fetch_add(field, value)
#endif

#endif /* HDR_ATOMIC_H__ */
