
include(CheckLibraryExists)
check_library_exists(m ceil "" HAVE_LIBM)
check_library_exists(rt clock_gettime "" HAVE_LIBRT)

# 32-bit ARM, RISC-V and others lower 64-bit __atomic builtins to libatomic calls
if (NOT MSVC)
    include(CheckCSourceCompiles)
    check_c_source_compiles("
#include <stdint.h>
int main(void)
{
    volatile int64_t x = 0;
    int64_t e = 0;
    __atomic_store_n(&x, 1, __ATOMIC_SEQ_CST);
    __atomic_exchange_n(&x, 2, __ATOMIC_SEQ_CST);
    __atomic_compare_exchange_n(&x, &e, 3, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return (int)(__atomic_add_fetch(&x, 1, __ATOMIC_SEQ_CST) + __atomic_load_n(&x, __ATOMIC_SEQ_CST));
}"
        HDR_ATOMIC64_LINKS)
    if (NOT HDR_ATOMIC64_LINKS)
        check_library_exists(atomic __atomic_fetch_add_8 "" HAVE_LIBATOMIC)
    endif()
endif()

if (HDR_LOG_ENABLED)
    set(HDR_LOG_IMPLEMENTATION hdr_histogram_log.c)
    set(HDR_ZLIB ZLIB::ZLIB)
    set(HDR_PACKED_CODEC_STUB "")
else()
    set(HDR_LOG_IMPLEMENTATION hdr_histogram_log_no_op.c)
    set(HDR_ZLIB "")
    set(HDR_PACKED_CODEC_STUB hdr_packed_histogram_no_op.c)
endif()

set(HDR_HISTOGRAM_SOURCES
    hdr_encoding.c
    hdr_histogram.c
    ${HDR_LOG_IMPLEMENTATION}
    hdr_packed_histogram.c
    ${HDR_PACKED_CODEC_STUB}
    hdr_interval_recorder.c
    hdr_thread.c
    hdr_time.c
    hdr_writer_reader_phaser.c)

set(HDR_HISTOGRAM_PRIVATE_HEADERS
    hdr_atomic.h
    hdr_encoding.h
    hdr_endian.h
    hdr_tests.h
    hdr_malloc.h)

# Minimal static core: just the record/percentile engine (hdr_histogram.c). No
# logging, interval recorder, phaser, zlib or threads. Allocator hooks
# (hdr_malloc.h) and the public API are preserved.
if(HDR_HISTOGRAM_CORE_ONLY)
    add_library(hdr_histogram_core_static STATIC
        hdr_histogram.c
        ${HDR_HISTOGRAM_PRIVATE_HEADERS}
        ${HDR_HISTOGRAM_PUBLIC_HEADERS})
    target_link_libraries(hdr_histogram_core_static
        PRIVATE $<$<BOOL:${HAVE_LIBM}>:m> $<$<BOOL:${HAVE_LIBATOMIC}>:atomic>)
    target_include_directories(hdr_histogram_core_static
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    add_library(${PROJECT_NAME}::hdr_histogram_core_static ALIAS hdr_histogram_core_static)
    install(
        TARGETS hdr_histogram_core_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    return()
endif()

function(hdr_histogram_add_library NAME LIBRARY_TYPE DO_INSTALL)
    add_library(${NAME} ${LIBRARY_TYPE}
        ${HDR_HISTOGRAM_SOURCES}
        ${HDR_HISTOGRAM_PRIVATE_HEADERS}
        ${HDR_HISTOGRAM_PUBLIC_HEADERS})

    target_compile_definitions(${NAME} PRIVATE
        $<$<BOOL:${HDR_LOG_ENABLED}>:HDR_PACKED_LOG_ENABLED>)

    target_link_libraries(${NAME}
        PRIVATE
#            ZLIB::ZLIB
            ${HDR_ZLIB}
#            $<$<BOOL:${ZLIB_FOUND}>ZLIB::ZLIB>
            Threads::Threads
            $<$<BOOL:${HAVE_LIBM}>:m>
            $<$<BOOL:${HAVE_LIBATOMIC}>:atomic>
            $<$<BOOL:${HAVE_LIBRT}>:rt>
            $<$<BOOL:${WIN32}>:ws2_32>)
    target_include_directories(
        ${NAME}
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    add_library(${PROJECT_NAME}::${NAME} ALIAS ${NAME})

    if(DO_INSTALL)
        install(
            TARGETS ${NAME}
            EXPORT ${PROJECT_NAME}-targets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endfunction()

option(HDR_HISTOGRAM_BUILD_SHARED "Build shared library" ON)
option(HDR_HISTOGRAM_INSTALL_SHARED "Install shared library" ON)
if(HDR_HISTOGRAM_BUILD_SHARED)
    hdr_histogram_add_library(hdr_histogram SHARED ${HDR_HISTOGRAM_INSTALL_SHARED})
    set_target_properties(hdr_histogram PROPERTIES
        VERSION ${HDR_VERSION}
        SOVERSION ${HDR_SOVERSION})
endif()

option(HDR_HISTOGRAM_BUILD_STATIC "Build static library" ON)
option(HDR_HISTOGRAM_INSTALL_STATIC "Install static library" ON)
if(HDR_HISTOGRAM_BUILD_STATIC)
    hdr_histogram_add_library(hdr_histogram_static STATIC ${HDR_HISTOGRAM_INSTALL_STATIC})
endif()
