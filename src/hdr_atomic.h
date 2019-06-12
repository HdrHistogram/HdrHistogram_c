/**
 * hdr_atomic.h
 * Written by Philip Orwig and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_ATOMIC_H__
#define HDR_ATOMIC_H__


#if defined(_MSC_VER)

#include <stdint.h>
#include <intrin.h>

static void __inline * hdr_atomic_load_pointer(void** pointer)
{
	_ReadBarrier();
	return *pointer;
}

static void hdr_atomic_store_pointer(void** pointer, void* value)
{
	_WriteBarrier();
	*pointer = value;
}

static int64_t __inline hdr_atomic_load_64(int64_t* field)
{ 
	_ReadBarrier();
	return *field;
}

static void __inline hdr_atomic_store_64(int64_t* field, int64_t value)
{
	_WriteBarrier();
	*field = value;
}

static int64_t __inline hdr_atomic_exchange_64(volatile int64_t* field, int64_t initial)
{
#if defined(_WIN64)
	return _InterlockedExchange64(field, initial);
#else
	return 0;
#endif
}

static int64_t __inline hdr_atomic_add_fetch_64(volatile int64_t* field, int64_t value)
{
#if defined(_WIN64)
	return _InterlockedExchangeAdd64(field, value) + value;
#else
    return 0;
#endif
}

#elif defined(__ATOMIC_SEQ_CST)

#define hdr_atomic_load_pointer(x) __atomic_load_n(x, __ATOMIC_SEQ_CST)
#define hdr_atomic_store_pointer(f,v) __atomic_store_n(f,v, __ATOMIC_SEQ_CST)
#define hdr_atomic_load_64(x) __atomic_load_n(x, __ATOMIC_SEQ_CST)
#define hdr_atomic_store_64(f,v) __atomic_store_n(f,v, __ATOMIC_SEQ_CST)
#define hdr_atomic_exchange_64(f,i) __atomic_exchange_n(f,i, __ATOMIC_SEQ_CST)
#define hdr_atomic_add_fetch_64(field, value) __atomic_add_fetch(field, value, __ATOMIC_SEQ_CST)

#elif defined(__x86_64__)

#include <stdint.h>

static inline void* hdr_atomic_load_pointer(void** pointer)
{
   void* p =  *pointer;
	asm volatile ("" ::: "memory");
	return p;
}

static inline void hdr_atomic_store_pointer(void** pointer, void* value)
{
    asm volatile ("lock; xchgq %0, %1" : "+q" (value), "+m" (*pointer));
}

static inline int64_t hdr_atomic_load_64(int64_t* field)
{
    int64_t i = *field;
	asm volatile ("" ::: "memory");
	return i;
}

static inline void hdr_atomic_store_64(int64_t* field, int64_t value)
{
    asm volatile ("lock; xchgq %0, %1" : "+q" (value), "+m" (*field));
}

static inline int64_t hdr_atomic_exchange_64(volatile int64_t* field, int64_t value)
{
    int64_t result = 0;
    asm volatile ("lock; xchgq %1, %2" : "=r" (result), "+q" (value), "+m" (*field));
    return result;
}

static inline int64_t hdr_atomic_add_fetch_64(volatile int64_t* field, int64_t value)
{
    return __sync_add_and_fetch(field, value);
}

#else

#error "Unable to determine atomic operations for your platform"

#endif

#endif /* HDR_ATOMIC_H__ */
