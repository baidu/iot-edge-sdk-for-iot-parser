set(DM_LIBRARY_NAME libbaidu-iot-dm)

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


FIND_LIBRARY(OPENSSL_LIB NAMES ssl libssl ssleay32
        HINTS ${OPENSSL_SEARCH_PATH}/lib ${OPENSSL_SEARCH_PATH}/lib64)

add_library(${DM_LIBRARY_NAME} SHARED
        device_management.c device_management.h
        device_management_conf.h)

target_link_libraries(${DM_LIBRARY_NAME} ${LIBUUID_LIBRARIES} ${LOG4C_LIBRARIES} ${CJSON_LIBRARIES} ${PAHO_LIBRARIES})