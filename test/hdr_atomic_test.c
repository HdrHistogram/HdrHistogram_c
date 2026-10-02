/**
 * hdr_histogram_test.c
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <stdint.h>

#include <stdio.h>
#include <hdr_atomic.h>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#include "minunit.h"

int tests_run = 0;

static char* test_store_load_64(void)
{
    int64_t value = 45;
    int64_t b;
    int64_t p = 0;

    hdr_atomic_store_64(&p, value);
    mu_assert("Failed hdr_atomic_store_64", compare_int64(p, value));

    b = hdr_atomic_load_64(&p);
    mu_assert("Failed hdr_atomic_load_64", compare_int64(p, b));

    return 0;
}

static char* test_store_load_pointer(void)
{
    int64_t r = 12;
    int64_t* q = 0;
    int64_t* s;

    hdr_atomic_store_pointer(&q, &r);
    mu_assert("Failed hdr_atomic_store_pointer", compare_int64(*q, r));

    s = hdr_atomic_load_pointer(&q);
    mu_assert("Failed hdr_atomic_load_pointer", compare_int64(*s, r));

    return 0;
}

static char* test_exchange(void)
{
    int64_t val1 = 123124;
    int64_t val2 = 987234;

    int64_t p = val1;
    int64_t q = val2;
    
    hdr_atomic_exchange_64(&p, q);
    mu_assert("Failed hdr_atomic_exchange_64", compare_int64(p, val2));

    return 0;
}

static char* test_add(void)
{
    int64_t val1 = 123124;
    int64_t val2 = 987234;
    int64_t expected = val1 + val2;

    int64_t result = hdr_atomic_add_fetch_64(&val1, val2);
    mu_assert("Failed hdr_atomic_exchange_64", compare_int64(result, expected));
    mu_assert("Failed hdr_atomic_exchange_64", compare_int64(val1, expected));

    return 0;
}

/* A reader must only ever see a whole value. The writer alternates all-zero and
   all-one bits, so a load that mixes halves of two stores returns neither. */
#define TEAR_ITERATIONS 5000000

static int64_t tear_field = 0;
static int64_t tear_observed = 0;

static void tear_write(void)
{
    int64_t i;
    for (i = 0; i < TEAR_ITERATIONS; i++)
    {
        hdr_atomic_store_64(&tear_field, (i & 1) ? INT64_C(-1) : INT64_C(0));
    }
}

static void tear_read(void)
{
    int64_t i;
    for (i = 0; i < TEAR_ITERATIONS; i++)
    {
        int64_t v = hdr_atomic_load_64(&tear_field);
        if (v != 0 && v != INT64_C(-1))
        {
            tear_observed++;
        }
    }
}

#if defined(_WIN32)
static unsigned __stdcall tear_write_main(void* arg) { (void) arg; tear_write(); return 0; }

static int run_tear_threads(void)
{
    HANDLE t = (HANDLE) _beginthreadex(NULL, 0, tear_write_main, NULL, 0, NULL);
    if (t == NULL)
    {
        return -1;
    }
    tear_read();
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return 0;
}
#else
static void* tear_write_main(void* arg) { (void) arg; tear_write(); return NULL; }

static int run_tear_threads(void)
{
    pthread_t t;
    if (pthread_create(&t, NULL, tear_write_main, NULL) != 0)
    {
        return -1;
    }
    tear_read();
    pthread_join(t, NULL);
    return 0;
}
#endif

static char* test_load_64_not_torn(void)
{
    tear_field = 0;
    tear_observed = 0;

    mu_assert("Failed to start thread", run_tear_threads() == 0);
    mu_assert("hdr_atomic_load_64 returned a torn value", compare_int64(tear_observed, 0));

    return 0;
}

static struct mu_result all_tests(void)
{
    mu_run_test(test_store_load_64);
    mu_run_test(test_store_load_pointer);
    mu_run_test(test_exchange);
    mu_run_test(test_add);
    mu_run_test(test_load_64_not_torn);

    mu_ok;
}

static int hdr_atomic_run_tests(void)
{
    struct mu_result result = all_tests();

    if (result.message != 0)
    {
        printf("hdr_atomic_test.%s(): %s\n", result.test, result.message);
    }
    else
    {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return result.message == NULL ? 0 : -1;
}

int main(void)
{
    return hdr_atomic_run_tests();
}
