find_package(PkgConfig)

pkg_check_modules(CHECK REQUIRED check)
find_path(CHECK_INCLUDE_DIR NAMES check.h PATHS ${CHECK_INCLUDE_DIRS})
find_library(CHECK_LIBRARY NAMES check PATHS ${CHECK_LIBRARY_DIRS})
include_directories(${CHECK_INCLUDE_DIR})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CHECK_LIBRARY ${CHECK_LIBRARY} rt m pthread subunit)
endif()

add_executable(parser_test parser_test.c)
target_link_libraries(parser_test ${CHECK_LIBRARY} gsl-parser_static)
target_compile_options(parser_test PRIVATE -Wno-pedantic)

# check-gsl-parser
add_custom_target(check-gsl-parser COMMENT "runs unit tests for gsl-parser project")
add_custom_command(TARGET check-gsl-parser POST_BUILD
    DEPENDS parser_test COMMENT "runs unit tests for parser module"
    COMMAND valgrind --leak-check=full ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/parser_test)
