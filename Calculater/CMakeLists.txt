cmake_minimum_required(VERSION 3.16)

project(FunctionsCalculater)

separate_arguments(
	LLVM_DEFINITIONS_LIST
	NATIVE_COMMAND
	${LLVM_DEFINITIONS}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
	add_compile_options(/utf-8)
endif()

llvm_map_components_to_libnames(LLVM_LIBS
	core
	support
	analysis
)

add_library(FunctionsCore STATIC
	token.h
	token.cc
)
target_include_directories(FunctionsCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(FunctionsEvaluator STATIC
	evaluator.h
	evaluator.cc
	runtime.h
)
target_link_libraries(FunctionsEvaluator PRIVATE FunctionsCore)
target_include_directories(FunctionsEvaluator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(FunctionsSemantic STATIC
	semantic.h
	semantic.cc
)
target_link_libraries(FunctionsSemantic PRIVATE FunctionsCore)
target_include_directories(FunctionsSemantic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(FunctionsCodegen STATIC
	codegen.h
	codegen.cc
)
target_link_libraries(FunctionsCodegen PRIVATE FunctionsCore ${LLVM_LIBS})
target_include_directories(FunctionsCodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(FunctionsCodegen SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_options(FunctionsCodegen PRIVATE ${LLVM_DEFINITIONS_LIST})

add_library(FunctionsFrontend STATIC
	scanner.h
	scanner.cc
)
target_link_libraries(FunctionsFrontend PRIVATE FunctionsCore FunctionsSemantic)
target_include_directories(FunctionsFrontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} main.cc)
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	FunctionsFrontend
)

add_executable(FunctionsUnitTest unit_test.cc)
target_link_libraries(FunctionsUnitTest
	PRIVATE
	FunctionsFrontend
	FunctionsEvaluator
	FunctionsCodegen
)

add_test(NAME FunctionsUnitTest COMMAND FunctionsUnitTest)
