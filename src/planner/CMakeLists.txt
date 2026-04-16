add_library(planner SHARED planner.cpp logical_operations.cpp physical_operations.cpp debug_planner.cpp)
target_include_directories(planner PUBLIC "${PLANNER_INCLUDE_DIR}")
target_link_libraries(planner PUBLIC parser lexer storage)
