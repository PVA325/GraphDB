add_library(storage SHARED
        cursor.cpp
        EdgeEntity/edge_index.cpp
        EdgeEntity/edge_manager.cpp
        EdgeEntity/edge_store.cpp
        graph_storage.cpp
        graph_db.cpp
        free_list.cpp
        metrics_store.cpp
        NodeEntity/node_index.cpp
        NodeEntity/node_manager.cpp
        NodeEntity/node_store.cpp
        page_cache.cpp
        string_interner.cpp
)
target_include_directories(storage PUBLIC "${CMAKE_SOURCE_DIR}/include")
