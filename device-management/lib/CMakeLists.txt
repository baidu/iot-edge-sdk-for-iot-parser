set(DM_LIBRARY_NAME baidu-iot-dm)

find_package(LibUUID REQUIRED)
include_directories(${LIBUUID_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LIBUUID_LIBRARIES})

find_package(Log4c REQUIRED)
include_directories(${LOG4C_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LOG4C_LIBRARIES})

find_package(cJSON REQUIRED)
include_directories(${CJSON_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CJSON_LIBRARIES})

find_package(Paho REQUIRED)
include_directories(${PAHO_INCLUDE_DIRS})
set(LIBS ${LIBS} ${PAHO_LIBRARIES})

find_package(Threads)

if (USE_SSL)
    find_package(OpenSSL REQUIRED)
    set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
endif (USE_SSL)

add_library(${DM_LIBRARY_NAME} SHARED
        device_management.c device_management.h
        device_management_util.c device_management_util.h
        device_management_conf.h)

target_link_libraries(${DM_LIBRARY_NAME} ${LIBUUID_LIBRARIES} ${LOG4C_LIBRARIES} ${CJSON_LIBRARIES} ${PAHO_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(
        ${DM_LIBRARY_NAME} PROPERTIES
        VERSION ${VERSION}
        SOVERSION ${VERSION_MAJOR})

INSTALL(FILES device_management.h DESTINATION include)
INSTALL(TARGETS ${DM_LIBRARY_NAME} ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
