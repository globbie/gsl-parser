cmake_minimum_required(VERSION 3.2)
project(gsl-parser C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_INSTALL_PREFIX /)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -03 -Werrror")

add_definitions(-D_POSIX_SOURCE=200809L)
add_definitions(-D_GNU_SOURCE)
add_definitions(-D_XOPEN_SOURCE=700)
add_definitions(-D_BSD_SOURCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

find_package(PkgConfig)

include_directories(include)

set(SOURCES src/parser.c include/gsl-parser.h src/spec.c)

add_library(${PROJECT_NAME}_obj OBJECT ${SOURCES})
add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)

add_subdirectory(tests)
