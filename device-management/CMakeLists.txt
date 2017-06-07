project (device-management)

cmake_minimum_required(VERSION 2.8)

set (VERSION 1.0.1)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

add_definitions (-DCMAKE -DVERSION=\"${VERSION}\")

add_subdirectory(lib)
add_subdirectory(samples)
add_subdirectory(test)
