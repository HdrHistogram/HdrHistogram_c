cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0042 NEW)

project("hdr_histogram")

ENABLE_TESTING()

SET (CMAKE_C_COMPILER "clang")

if(UNIX)
    set(CMAKE_C_FLAGS "-Wall -std=gnu99")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

include_directories("${CMAKE_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_SOURCE_DIR}/test")
add_subdirectory("${CMAKE_SOURCE_DIR}/examples")

SET(CPACK_GENERATOR "TGZ")

INCLUDE(CPack)
