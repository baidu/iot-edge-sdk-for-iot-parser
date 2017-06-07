include_directories(${device-management_SOURCE_DIR}/lib)
link_directories(${device-management_BINARY_DIR}/lib)


add_executable(tea-pot-example tea_pot.c)

add_executable(smart-bubble-example cpp/smart_bubble.cpp)

target_link_libraries(tea-pot-example baidu-iot-dm)

target_link_libraries(smart-bubble-example baidu-iot-dm)