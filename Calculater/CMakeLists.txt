cmake_minimum_required(VERSION 3.16)

project(FunctionsCalculater)

separate_arguments(
	LLVM_DEFINITIONS_LIST
	NATIVE_COMMAND
	${LLVM_DEFINITIONS}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 语言特性开关：函数体内能否定义函数（默认关闭）。
# 语义库是该判定的唯一所有者，其余目标通过 PUBLIC 定义继承同一取值。
option(FC_ENABLE_NESTED_FUNCTIONS
	"Allow function definitions inside function bodies (frontend only)"
	OFF)

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
	lexer.h
	lexer.cc
	trace.h
	trace.cc
	parser.h
	parser.cc
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
	language_options.h
)
target_link_libraries(FunctionsSemantic PRIVATE FunctionsCore)
target_include_directories(FunctionsSemantic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(FunctionsSemantic PUBLIC
	$<$<BOOL:${FC_ENABLE_NESTED_FUNCTIONS}>:FC_ENABLE_NESTED_FUNCTIONS=1>)

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
# 前端对外暴露 semantic.h（scanner.h 内含），故链接与语言开关定义需 PUBLIC 传递
target_link_libraries(FunctionsFrontend PUBLIC FunctionsCore FunctionsSemantic)
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

add_executable(ShadowRepro shadow_repro.cc)
target_link_libraries(ShadowRepro PRIVATE FunctionsFrontend FunctionsEvaluator)
