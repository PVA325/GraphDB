add_executable(physical_storage_tests physical_storage_tests.cpp)
target_link_libraries(physical_storage_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        planner
)

target_include_directories(physical_storage_tests
        PUBLIC
        "${PLANNER_INCLUDE_DIR}"
        "${STORAGE_INCLUDE_DIR}"
        "${PARSER_INCLUDE_DIR}"
)