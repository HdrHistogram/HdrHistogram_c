cmake_minimum_required(VERSION 2.8)

if("${CMAKE_VERSION}" VERSION_GREATER 3.0.0)
    cmake_policy(SET CMP0042 NEW)
endif()

project("hdr_histogram")

ENABLE_TESTING()

if(UNIX)
    set(CMAKE_C_FLAGS "-Wall -Wno-unknown-pragmas -std=gnu99")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -g")
endif()

include_directories("${CMAKE_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_SOURCE_DIR}/test")
add_subdirectory("${CMAKE_SOURCE_DIR}/examples")

SET(CPACK_GENERATOR "TGZ")
SET(CPACK_PACKAGE_VERSION "0.9.1")

INCLUDE(CPack)
