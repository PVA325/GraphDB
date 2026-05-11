cmake_minimum_required(VERSION 3.25)
project(GraphDB)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

enable_testing()

add_subdirectory(src)

add_subdirectory(tests)

