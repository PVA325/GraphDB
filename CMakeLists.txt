cmake_minimum_required(VERSION 3.25)
project(GraphDB)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(PARSER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/parser")
#set(PLANNER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/planner")
#set(STORAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/storage")

add_subdirectory(src)

add_subdirectory(tests)

add_executable(tmp src/tmp_main.cpp)
target_link_libraries(tmp PRIVATE planner)
