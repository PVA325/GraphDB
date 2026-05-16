add_subdirectory(parser)
add_subdirectory(storage)
add_subdirectory(planner)

add_executable(graphdb runner.cpp)
target_link_libraries(graphdb PRIVATE planner)
