add_library(planner SHARED query_planner.cpp logical_operations.cpp physical_operations.cpp debug_planner.cpp cost_model.cpp optimizer.cpp)
target_include_directories(planner PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(planner PUBLIC parser lexer storage)
