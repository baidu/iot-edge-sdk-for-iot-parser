# - Try to find PAHO
# Once done this will define
#
#  PAHO_FOUND - system has PAHO
#  PAHO_INCLUDE_DIRS - the PAHO include directory
#  PAHO_LIBRARIES - Link these to use PAHO
#  PAHO_DEFINITIONS - Compiler switches required for using PAHO
#
#  Copyright (c) 2009 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (PAHO_LIBRARIES AND PAHO_INCLUDE_DIRS)
    # in cache already
    set(PAHO_FOUND TRUE)
else (PAHO_LIBRARIES AND PAHO_INCLUDE_DIRS)

    find_path(PAHO_INCLUDE_DIR
            NAMES
            MQTTAsync.h MQTTClient.h
            PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
            )
    mark_as_advanced(PAHO_INCLUDE_DIR)

    if (USE_SSL)
        find_library(PAHO_LIBRARY
                NAMES
                paho-mqtt3cs
                PATHS
                /usr/lib
                /usr/local/lib
                /opt/local/lib
                /sw/lib
                )
        mark_as_advanced(PAHO_LIBRARY)

        find_library(PAHOA_LIBRARY
                NAMES
                paho-mqtt3as
                PATHS
                /usr/lib
                /usr/local/lib
                /opt/local/lib
                /sw/lib
                )
        mark_as_advanced(PAHOA_LIBRARY)
    else (USE_SSL)
        find_library(PAHO_LIBRARY
                NAMES
                paho-mqtt3c
                PATHS
                /usr/lib
                /usr/local/lib
                /opt/local/lib
                /sw/lib
                )
        mark_as_advanced(PAHO_LIBRARY)

        find_library(PAHOA_LIBRARY
                NAMES
                paho-mqtt3a
                PATHS
                /usr/lib
                /usr/local/lib
                /opt/local/lib
                /sw/lib
                )
        mark_as_advanced(PAHOA_LIBRARY)
    endif (USE_SSL)

    if (PAHO_LIBRARY)
        set(PAHO_FOUND TRUE CACHE INTERNAL "Wether the PAHO library has been found" FORCE)
    endif (PAHO_LIBRARY)

    set(PAHO_INCLUDE_DIRS
            ${PAHO_INCLUDE_DIR}
            )

    if (PAHO_FOUND)
        set(PAHO_LIBRARIES
                ${PAHO_LIBRARIES}
                ${PAHO_LIBRARY}
                ${PAHOA_LIBRARY}
                )
    endif (PAHO_FOUND)

    if (PAHO_INCLUDE_DIRS AND PAHO_LIBRARIES)
        set(PAHO_FOUND TRUE)
    endif (PAHO_INCLUDE_DIRS AND PAHO_LIBRARIES)

    if (PAHO_FOUND)
        message(STATUS "Found PAHO: ${PAHO_LIBRARIES}")
    else (PAHO_FOUND)
        message(FATAL_ERROR "Could not find PAHO")
    endif (PAHO_FOUND)

    # show the PAHO_INCLUDE_DIRS and PAHO_LIBRARIES variables only in the advanced view
    mark_as_advanced(PAHO_INCLUDE_DIRS PAHO_LIBRARIES)

endif (PAHO_LIBRARIES AND PAHO_INCLUDE_DIRS)
