include_directories(${device-management_SOURCE_DIR}/lib)
link_directories(${device-management_BINARY_DIR}/lib)


add_executable(tea-pot-example tea_pot.c)

target_link_libraries(tea-pot-example libbaidu-iot-dm)