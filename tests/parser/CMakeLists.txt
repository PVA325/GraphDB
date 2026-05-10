add_executable(ast_tests ast_tests.cpp)
target_link_libraries(ast_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        parser
        lexer
)
target_include_directories(ast_tests PRIVATE "${CMAKE_SOURCE_DIR}/include")

include(GoogleTest)

add_test(NAME ast_tests COMMAND $<TARGET_FILE:ast_tests>)
