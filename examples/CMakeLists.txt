add_executable(hdr_decoder hdr_decoder.c)
if (WIN32)
    target_link_libraries(hdr_decoder hdr_histogram_static ${ZLIB_LIBRARIES})
else()
    target_link_libraries(hdr_decoder hdr_histogram m rt z)
endif()

install(TARGETS hdr_decoder DESTINATION bin)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_executable(hiccup hiccup.c)
    target_link_libraries(hiccup hdr_histogram m z pthread rt)
    install(TARGETS hiccup DESTINATION bin)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
