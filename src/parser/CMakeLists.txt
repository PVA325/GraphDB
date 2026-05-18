add_library(parser SHARED parser.cpp ast_utility.cpp ast_expression.cpp ast_pattern.cpp ast_debug_string.cpp ../planner/physical/slotsmapping.cpp)
target_include_directories(parser
  PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

add_library(lexer SHARED lexer.cpp)
target_include_directories(lexer
  PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
