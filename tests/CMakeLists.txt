set(TEST_BINARY ${PROJECT_NAME}_tests)
set(TEST_SOURCES run_tests.c)

add_executable(${TEST_BINARY} ${TEST_SOURCES})

target_link_libraries(${TEST_BINARY} ${CHECK_LIBRARY} ${PROJECT_NAME}_static)
