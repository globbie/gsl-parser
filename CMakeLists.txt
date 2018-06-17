cmake_minimum_required(VERSION 3.2)
project(gsl-parser C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_INSTALL_PREFIX /)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")
message("")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Werror")

include_directories(include)

set(HEADERS include/gsl-parser.h include/gsl-parser/config.h include/gsl-parser/gsl_err.h
        include/gsl-parser/gsl_log.h include/gsl-parser/gsl_task_spec.h)
set(SOURCES src/parser.c)

add_library(${PROJECT_NAME}_obj OBJECT ${HEADERS} ${SOURCES})
add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)

add_subdirectory(tests)
