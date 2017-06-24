
set(SOURCES main.c)

add_executable(tests ${SOURCES})

target_link_libraries(tests gsl-parser_static)