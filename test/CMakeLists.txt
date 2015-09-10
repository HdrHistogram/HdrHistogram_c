INCLUDE(CheckLibraryExists)

add_executable(hdr_histogram_test hdr_histogram_test.c)
add_executable(hdr_histogram_log_test hdr_histogram_log_test.c)

add_executable(perftest hdr_histogram_perf.c)

target_link_libraries(hdr_histogram_test hdr_histogram m)
target_link_libraries(hdr_histogram_log_test hdr_histogram m z)
target_link_libraries(perftest hdr_histogram m z)

CHECK_LIBRARY_EXISTS(rt clock_gettime "" RT_EXISTS)
if (RT_EXISTS)
    target_link_libraries(hdr_histogram_log_test rt)
    target_link_libraries(perftest rt)
endif (RT_EXISTS)

install(TARGETS hdr_histogram_test DESTINATION bin)
install(TARGETS hdr_histogram_log_test DESTINATION bin)
install(TARGETS perftest DESTINATION bin)

add_test(Histogram hdr_histogram_test)
add_test(HistogramLogging hdr_histogram_log_test)

configure_file(jHiccup-2.0.1.logV0.hlog jHiccup-2.0.1.logV0.hlog COPYONLY)
configure_file(jHiccup-2.0.6.logV1.hlog jHiccup-2.0.6.logV1.hlog COPYONLY)
configure_file(jHiccup-2.0.7S.logV2.hlog jHiccup-2.0.7S.logV2.hlog COPYONLY)
