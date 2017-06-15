project(device-management)

cmake_minimum_required(VERSION 2.8)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 1)
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(CMAKE_INSTALL_LIBDIR "/usr/local/lib")
set(CMAKE_INSTALL_INCLUDEDIR "/usr/local/include")
set(USE_SSL 1) # comment this line if you want to disable MQTT over SSL.

add_definitions(-DCMAKE -DVERSION=\"${VERSION}\")

add_subdirectory(lib)
add_subdirectory(samples)
add_subdirectory(test)
