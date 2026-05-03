add_executable(logical_planner_tests logical_debug_string_tests.cpp logical_tests.cpp)
target_link_libraries(logical_planner_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        planner
)
target_include_directories(logical_planner_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_executable(physical_planner_tests physical_planner_tests.cpp hash_join_tests.cpp)
target_link_libraries(physical_planner_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        planner
)
target_include_directories(physical_planner_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_executable(optimizer_planner_tests optimizer_tests.cpp)
target_link_libraries(optimizer_planner_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        planner
)
target_include_directories(optimizer_planner_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")