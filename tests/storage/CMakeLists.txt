add_executable(all_base_storage_tests base_ram_mode_storage_tests.cpp base_not_ram_mode_storage_tests.cpp)
target_link_libraries(all_base_storage_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        planner
)

target_include_directories(all_base_storage_tests
        PUBLIC
        "${PLANNER_INCLUDE_DIR}"
        "${STORAGE_INCLUDE_DIR}"
        "${PARSER_INCLUDE_DIR}"
)


include(GoogleTest)

add_test(NAME all_base_storage_tests COMMAND $<TARGET_FILE:all_base_storage_tests>)
