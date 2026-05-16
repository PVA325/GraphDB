cmake_minimum_required(VERSION 3.25)
project(GraphDB)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED CONFIG)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

enable_testing()

add_subdirectory(src)

add_subdirectory(tests)

set(GRAPHDB_INSTALL_BIN_DIR bin)
set(GRAPHDB_INSTALL_LIB_DIR lib)
set(GRAPHDB_INSTALL_INCLUDE_DIR include)
set(GRAPHDB_CONFIG_INSTALL_DIR ${GRAPHDB_INSTALL_LIB_DIR}/cmake/GraphDB)

install(TARGETS graphdb
  RUNTIME DESTINATION ${GRAPHDB_INSTALL_BIN_DIR}
)

install(TARGETS planner parser lexer storage
  EXPORT GraphDBTargets
  LIBRARY DESTINATION ${GRAPHDB_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${GRAPHDB_INSTALL_LIB_DIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION ${GRAPHDB_INSTALL_INCLUDE_DIR}
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.tpp"
)

install(EXPORT GraphDBTargets
  FILE GraphDBTargets.cmake
  NAMESPACE GraphDB::
  DESTINATION ${GRAPHDB_CONFIG_INSTALL_DIR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/GraphDBConfigVersion.cmake"
  VERSION 1.0.0
  COMPATIBILITY AnyNewerVersion
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GraphDBConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/GraphDBConfig.cmake"
  @ONLY
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/GraphDBConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/GraphDBConfigVersion.cmake"
  DESTINATION ${GRAPHDB_CONFIG_INSTALL_DIR}
)


add_custom_target(install-clean
  COMMAND ${CMAKE_COMMAND} -E echo "Removing installed GraphDB files under ${CMAKE_INSTALL_PREFIX}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${GRAPHDB_INSTALL_BIN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${GRAPHDB_INSTALL_LIB_DIR}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${GRAPHDB_INSTALL_INCLUDE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${GRAPHDB_CONFIG_INSTALL_DIR}"
  COMMENT "install-clean: remove installed files (use with care)"
)


