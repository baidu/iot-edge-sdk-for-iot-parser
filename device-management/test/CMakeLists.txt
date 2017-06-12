find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTEST_LIBRARIES})

find_package(PahoCpp REQUIRED)
include_directories(${PAHOCPP_INCLUDE_DIRS})
set(LIBS ${LIBS} ${PAHOCPP_LIBRARIES})

include_directories(${device-management_SOURCE_DIR}/lib)
link_directories(${device-management_BINARY_DIR}/lib)

add_executable(update-test update_test.cpp smart_birdcage.cpp smart_birdcage.h device_management_stub.cpp device_management_stub.h)

target_link_libraries(update-test baidu-iot-dm gtest gtest_main ${PAHOCPP_LIBRARIES})