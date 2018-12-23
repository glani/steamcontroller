cmake_minimum_required(VERSION 3.1)
project(sclib C)

set(CMAKE_C_STANDARD 99)

set(SC_LIB sclib)

IF (CMAKE_COMPILER_IS_GNUCC)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
ELSEIF (MSVC)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
ENDIF ()

SET(SOURCES
        steamcontroller_feedback.c
        steamcontroller_setup.c
        steamcontroller_state.c
        steamcontroller_wireless.c
        )

if (UNIX)
    list(APPEND SOURCES steamcontroller_linux.c)
ELSEIF (WIN32)
    if (MINGW)
        add_definitions(-D_USE_LIBUSB)
        include_directories(
                ${CMAKE_CURRENT_SOURCE_DIR}/../../libusb/include/libusb-1.0
        )
        #test purposes
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libusb/MinGW64/static/)
        list(APPEND SOURCES steamcontroller_libusb.c)
    ELSEIF (MSVC)
        list(APPEND SOURCES steamcontroller_win32.c)
    ENDIF ()
ENDIF ()


add_library(${SC_LIB} STATIC ${SOURCES})
IF (WIN32)
    if (MINGW)
        add_definitions(-D_USE_LIBUSB)
        TARGET_LINK_LIBRARIES(${SC_LIB} usb-1.0)
        list(APPEND SOURCES)
    ELSEIF (MSVC)
        TARGET_LINK_LIBRARIES(${SC_LIB} setupapi hid)
    ENDIF ()
ENDIF ()


ADD_EXECUTABLE          ( SteamControllerExample sclibexample.c)
TARGET_LINK_LIBRARIES   ( SteamControllerExample ${SC_LIB} )

