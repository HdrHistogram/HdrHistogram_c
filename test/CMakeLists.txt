INCLUDE(CheckLibraryExists)
INCLUDE(ExternalProject)

option(HDR_HISTOGRAM_BUILD_BENCHMARK "Build benchmark" OFF)

if (HDR_HISTOGRAM_BUILD_BENCHMARK)
    if (UNIX)
        ExternalProject_Add(
                google_benchmark
                URL ${CMAKE_CURRENT_SOURCE_DIR}/../lib/benchmark-1.5.0.zip
                URL_MD5 1af48846917b9730bee1848f6e38f563
                CMAKE_ARGS -DBENCHMARK_ENABLE_TESTING=OFF -DBENCHMARK_ENABLE_INSTALL=OFF
                SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark-src"
                BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark-build"
                INSTALL_COMMAND   ""
        )

        ExternalProject_Get_Property(google_benchmark source_dir)
        set(GOOGLE_BENCHMARK_SOURCE_DIR ${source_dir})
        ExternalProject_Get_Property(google_benchmark binary_dir)
        set(GOOGLE_BENCHMARK_BINARY_DIR ${binary_dir})

        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)
        add_executable(hdr_histogram_benchmark hdr_histogram_benchmark.cpp)
        target_include_directories(hdr_histogram_benchmark PUBLIC ${GOOGLE_BENCHMARK_SOURCE_DIR}/include)
        target_link_directories(hdr_histogram_benchmark PUBLIC ${GOOGLE_BENCHMARK_BINARY_DIR}/src)
        target_link_libraries(hdr_histogram_benchmark benchmark)
        target_link_libraries(hdr_histogram_benchmark hdr_histogram_static m z pthread)
    else()
        message(WARNING
              "google.benchmark - microbenchmarks disabled on WIN32 platforms")
    endif()
endif()

add_executable(hdr_histogram_test hdr_histogram_test.c minunit.c)
add_executable(hdr_histogram_atomic_test hdr_histogram_atomic_test.c minunit.c)
add_executable(hdr_histogram_log_test hdr_histogram_log_test.c minunit.c)
add_executable(hdr_atomic_test hdr_atomic_test.c minunit.c hdr_test_util.h)

add_executable(perftest hdr_histogram_perf.c)

if (WIN32)
    add_library(z STATIC IMPORTED)
    set_property(TARGET z PROPERTY IMPORTED_LOCATION ${ZLIB_LIBRARIES})

    target_link_libraries(hdr_histogram_test hdr_histogram_static ws2_32)
    target_link_libraries(hdr_histogram_atomic_test hdr_histogram_static ws2_32)
    target_link_libraries(hdr_histogram_log_test hdr_histogram_static z ws2_32)
    target_link_libraries(perftest hdr_histogram_static z ws2_32)
    target_link_libraries(hdr_atomic_test z ws2_32)
else()
    target_link_libraries(hdr_histogram_test hdr_histogram_static m)
    target_link_libraries(hdr_histogram_atomic_test hdr_histogram_static m)
    target_link_libraries(hdr_histogram_log_test hdr_histogram_static m z)
    target_link_libraries(perftest hdr_histogram_static m z)
    target_link_libraries(hdr_atomic_test z)
endif()

CHECK_LIBRARY_EXISTS(rt clock_gettime "" RT_EXISTS)
if (RT_EXISTS)
    target_link_libraries(hdr_histogram_log_test rt)
    target_link_libraries(perftest rt)
endif (RT_EXISTS)

install(TARGETS hdr_histogram_test DESTINATION bin)
install(TARGETS hdr_histogram_atomic_test DESTINATION bin)
install(TARGETS hdr_histogram_log_test DESTINATION bin)
install(TARGETS perftest DESTINATION bin)

add_test(Histogram hdr_histogram_test)
add_test(HistogramAtomic hdr_histogram_atomic_test)
add_test(HistogramLogging hdr_histogram_log_test)
add_test(Atomic hdr_atomic_test)

if (UNIX)
    add_executable(hdr_histogram_atomic_concurrency_test hdr_histogram_atomic_concurrency_test.c minunit.c)
    target_link_libraries(hdr_histogram_atomic_concurrency_test hdr_histogram_static m pthread)
    add_test(HistogramAtomicConcurrency hdr_histogram_atomic_concurrency_test)
endif()

configure_file(jHiccup-2.0.1.logV0.hlog jHiccup-2.0.1.logV0.hlog COPYONLY)
configure_file(jHiccup-2.0.6.logV1.hlog jHiccup-2.0.6.logV1.hlog COPYONLY)
configure_file(jHiccup-2.0.7S.logV2.hlog jHiccup-2.0.7S.logV2.hlog COPYONLY)
configure_file(jHiccup-2.0.7S.logV3.hlog jHiccup-2.0.7S.logV3.hlog COPYONLY)
