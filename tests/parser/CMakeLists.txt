add_executable(ast_unit_tests ast_unit_tests.cpp)
target_link_libraries(ast_unit_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        parser
        lexer
)
target_include_directories(ast_unit_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_executable(parser_integration_tests parser_integration_tests.cpp)
target_link_libraries(parser_integration_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        parser
        lexer
)
target_include_directories(parser_integration_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_executable(ast_debug_string_tests ast_debug_string_tests.cpp
        ast_debug_string_tests.cpp)
target_link_libraries(ast_debug_string_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        parser
)
target_include_directories(ast_debug_string_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

include(GoogleTest)

add_test(NAME ast_unit_tests COMMAND $<TARGET_FILE:ast_unit_tests>)
add_test(NAME parser_integration_tests COMMAND $<TARGET_FILE:parser_integration_tests>)
add_test(NAME ast_debug_string_tests COMMAND $<TARGET_FILE:ast_debug_string_tests>)
