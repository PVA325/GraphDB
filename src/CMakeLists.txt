add_subdirectory(parser)
add_subdirectory(storage)
add_subdirectory(planner)

add_executable(runner runner.cpp)
target_link_libraries(runner PRIVATE planner)