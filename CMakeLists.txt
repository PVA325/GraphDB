cmake_minimum_required(VERSION 3.25)
project(GraphDB)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/parser)
add_subdirectory(src/planner)
add_subdirectory(src/storage)

add_executable(tmp src/tmp_main.cpp)
target_link_libraries(tmp PRIVATE planner)
