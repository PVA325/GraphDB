add_library(planner SHARED
        query_planner.cpp
        utils_planner.cpp

        logical/scan.cpp
        logical/expand.cpp
        logical/filter.cpp
        logical/project.cpp
        logical/limit.cpp
        logical/sort.cpp
        logical/join.cpp
        logical/create.cpp
        logical/set.cpp
        logical/delete.cpp

        physical/slotsmapping.cpp
        physical/physical_utils.cpp

        physical/cursors/node_scan_cursor.cpp
        physical/cursors/node_property_filter_cursor.cpp
        physical/cursors/filter_cursor.cpp
        physical/cursors/project_cursor.cpp
        physical/cursors/limit_cursor.cpp
        physical/cursors/nested_loop_join_cursor.cpp
        physical/cursors/hash_join_cursor.cpp
        physical/cursors/set_cursor.cpp
        physical/cursors/create_cursor.cpp
        physical/cursors/delete_cursor.cpp
        physical/cursors/sort_cursor.cpp

        physical/operations/index_seek.cpp
        physical/operations/node_scan.cpp
        physical/operations/node_property_filter.cpp
        physical/operations/filter.cpp
        physical/operations/project.cpp
        physical/operations/limit.cpp
        physical/operations/nested_loop_join.cpp
        physical/operations/hash_join.cpp
        physical/operations/set.cpp
        physical/operations/create.cpp
        physical/operations/delete.cpp
        physical/operations/sort.cpp

        physical_operations.cpp
        debug_planner.cpp
        cost_model.cpp
        optimizer.cpp
)
target_include_directories(planner PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(planner PUBLIC parser lexer storage)
