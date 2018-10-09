project(gsl-parser_tests C)

find_package(PkgConfig)

pkg_check_modules(CHECK REQUIRED check)
find_path(CHECK_INCLUDE_DIR NAMES check.h PATHS ${CHECK_INCLUDE_DIRS})
find_library(CHECK_LIBRARY NAMES check PATHS ${CHECK_LIBRARY_DIRS})
include_directories(${CHECK_INCLUDE_DIR})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CHECK_LIBRARY ${CHECK_LIBRARY} rt m pthread subunit)
endif()

set(SOURCES run_tests.c)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE -Wno-pedantic)
target_link_libraries(${PROJECT_NAME} ${CHECK_LIBRARY} gsl-parser_static)

add_test(NAME mytest COMMAND ${PROJECT_NAME})
