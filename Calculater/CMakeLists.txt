cmake_minimum_required(VERSION 3.16)

project(FunctionsCalculater)

set(SRC_LST
	"token.h"
	"token.cc"
	"scanner.h"
	"scanner.cc"
	"main.cc"
)

separate_arguments(
	LLVM_DEFINITIONS_LIST
	NATIVE_COMMAND
	${LLVM_DEFINITIONS}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(${PROJECT_NAME} ${SRC_LST})

add_executable(FunctionCallASTTest
	"token.h"
	"token.cc"
	"scanner.h"
	"scanner.cc"
	"call_ast_test.cc"
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${LLVM_INCLUDE_DIRS}
)

llvm_map_components_to_libnames(LLVM_LIBS
	core
	support
	analysis
)

target_link_libraries(
	${PROJECT_NAME} PUBLIC 
	${LLVM_LIBS})

target_include_directories(FunctionCallASTTest
	PRIVATE
	${LLVM_INCLUDE_DIRS}
)

target_link_libraries(FunctionCallASTTest PRIVATE ${LLVM_LIBS})

add_test(NAME FunctionCallASTTest COMMAND FunctionCallASTTest)
