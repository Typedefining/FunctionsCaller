cmake_minimum_required(VERSION 3.16)

project(FunctionsCalculater)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)

set(SRC_LST
	"ui_calculaterUI.h"
	"calculater.h"
	"calculater.cc"
	"token.h"
	"token.cc"
	"scanner.h"
	"scanner.cc"
	"functionmanager.h"
	"functionmanager.cc"
	"preimpifunc.h"
	"preimpifunc.cc"
	"main.cc"
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(${PROJECT_NAME} ${SRC_LST})

target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets )