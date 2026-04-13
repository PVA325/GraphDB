set(PARSER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/parser")

add_library(parser SHARED parser.cpp)
target_include_directories(parser PUBLIC "${PARSER_INCLUDE_DIR}")

add_library(lexer SHARED lexer.cpp)
target_include_directories(lexer PUBLIC "${PARSER_INCLUDE_DIR}")
