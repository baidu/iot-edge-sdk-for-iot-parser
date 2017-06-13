include_directories(${device-management_SOURCE_DIR}/lib)

link_directories(${device-management_BINARY_DIR}/lib)

add_executable(tea-pot-example tea_pot.c)

add_executable(pump-example pump.c)

target_link_libraries(tea-pot-example baidu-iot-dm)

target_link_libraries(pump-example baidu-iot-dm)
