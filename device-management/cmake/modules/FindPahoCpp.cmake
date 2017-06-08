# - Try to find PAHOCPP
# Once done this will define
#
#  PAHOCPP_FOUND - system has PAHOCPP
#  PAHOCPP_INCLUDE_DIRS - the PAHOCPP include directory
#  PAHOCPP_LIBRARIES - Link these to use PAHOCPP
#  PAHOCPP_DEFINITIONS - Compiler switches required for using PAHOCPP
#
#  Copyright (c) 2009 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (PAHOCPP_LIBRARIES AND PAHOCPP_INCLUDE_DIRS)
    # in cache already
    set(PAHOCPP_FOUND TRUE)
else (PAHOCPP_LIBRARIES AND PAHOCPP_INCLUDE_DIRS)

    find_path(PAHOCPP_INCLUDE_DIR
            NAMES
            mqtt/async_client.h
            PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
            )
    mark_as_advanced(PAHOCPP_INCLUDE_DIR)

    find_library(PAHOCPP_LIBRARY
            NAMES
            paho-mqttpp3
            PATHS
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
            )
    mark_as_advanced(PAHOCPP_LIBRARY)

    if (PAHOCPP_LIBRARY)
        set(PAHOCPP_FOUND TRUE CACHE INTERNAL "Wether the PAHOCPP library has been found" FORCE)
    endif (PAHOCPP_LIBRARY)

    set(PAHOCPP_INCLUDE_DIRS
            ${PAHOCPP_INCLUDE_DIR}
            )

    if (PAHOCPP_FOUND)
        set(PAHOCPP_LIBRARIES
                ${PAHOCPP_LIBRARIES}
                ${PAHOCPP_LIBRARY}
                )
    endif (PAHOCPP_FOUND)

    if (PAHOCPP_INCLUDE_DIRS AND PAHOCPP_LIBRARIES)
        set(PAHOCPP_FOUND TRUE)
    endif (PAHOCPP_INCLUDE_DIRS AND PAHOCPP_LIBRARIES)

    if (PAHOCPP_FOUND)
        message(STATUS "Found PAHOCPP: ${PAHOCPP_LIBRARIES}")
    else (PAHOCPP_FOUND)
        message(FATAL_ERROR "Could not find PAHOCPP")
    endif (PAHOCPP_FOUND)

    # show the PAHOCPP_INCLUDE_DIRS and PAHOCPP_LIBRARIES variables only in the advanced view
    mark_as_advanced(PAHOCPP_INCLUDE_DIRS PAHOCPP_LIBRARIES)

endif (PAHOCPP_LIBRARIES AND PAHOCPP_INCLUDE_DIRS)
