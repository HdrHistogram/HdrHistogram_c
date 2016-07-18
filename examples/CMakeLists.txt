add_executable(hdr_decoder hdr_decoder.c)
if (WIN32)
    target_link_libraries(hdr_decoder hdr_histogram_static)
else()
    target_link_libraries(hdr_decoder hdr_histogram m z)
endif()

CHECK_LIBRARY_EXISTS(rt clock_gettime "" RT_EXISTS)
if (RT_EXISTS)
    target_link_libraries(hdr_decoder rt)
endif (RT_EXISTS)

install(TARGETS hdr_decoder DESTINATION bin)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_executable(hiccup hiccup.c)
    target_link_libraries(hiccup hdr_histogram m z pthread rt)
    install(TARGETS hiccup DESTINATION bin)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
