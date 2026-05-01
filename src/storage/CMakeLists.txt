add_library(storage SHARED
        api_storage.cpp
        classes_cursor_storage.cpp
        crud_edges.cpp
        crud_nodes.cpp
        cursor_methods_storage.cpp
	load_storage.cpp
	save_storage.cpp
)
target_include_directories(storage PUBLIC "${CMAKE_SOURCE_DIR}/include")
