find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTEST_LIBRARIES})

find_package(GMock REQUIRED)
include_directories(${GMOCK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GMOCK_LIBRARIES})

#find_package(PahoCpp REQUIRED)
#include_directories(${PAHOCPP_INCLUDE_DIRS})
#set(LIBS ${LIBS} ${PAHOCPP_LIBRARIES})

include_directories(${device-management_SOURCE_DIR}/lib)
link_directories(${device-management_BINARY_DIR}/lib)

add_executable(integration-test test_conf.h test_conf.cpp integration_test.cpp device_management_stub.cpp device_management_stub.h test_util.cpp test_util.h)

#target_link_libraries(integration-test baidu-iot-dm gtest gtest_main ${PAHOCPP_LIBRARIES})
target_link_libraries(integration-test baidu-iot-dm gtest gtest_main gmock)
