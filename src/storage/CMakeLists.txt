add_library(storage SHARED
        api_storage.cpp
        classes_cursor_storage.cpp
        crud_edges.cpp
        crud_nodes.cpp
        cursor_methods_storage.cpp
)
target_include_directories(storage PUBLIC "${STORAGE_INCLUDE_DIR}")

