add_library(parser SHARED parser.cpp)
target_include_directories(parser PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(parser_utility SHARED parser_utility.cpp)
target_include_directories(parser_utility PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(ast_utility SHARED ast_utility.cpp)
target_include_directories(ast_utility PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(ast_expression SHARED ast_expression.cpp)
target_include_directories(ast_expression PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(ast_pattern SHARED ast_pattern.cpp)
target_include_directories(ast_pattern PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(ast_debug_string SHARED ast_debug_string.cpp)
target_include_directories(ast_debug_string PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(lexer SHARED lexer.cpp)
target_include_directories(lexer PUBLIC "${CMAKE_SOURCE_DIR}/include")
