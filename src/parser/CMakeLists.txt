add_library(parser SHARED parser.cpp)
target_include_directories(parser PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_library(lexer SHARED lexer.cpp)
target_include_directories(lexer PUBLIC "${CMAKE_SOURCE_DIR}/include")
