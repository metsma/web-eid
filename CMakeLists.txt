cmake_minimum_required(VERSION 3.1.0)

project(web-eid)

if(NOT DEFINED ENV{QTDIR})
	message(FATAL_ERROR "QTDIR environment variable is not set!")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH $ENV{QTDIR}/lib/cmake)

find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5Network CONFIG REQUIRED)
find_package(Qt5WebSockets CONFIG REQUIRED)
find_package(Qt5Concurrent CONFIG REQUIRED)
find_package(Qt5Svg CONFIG REQUIRED)

file(GLOB web-eid_SRCS src/*.cpp src/dialogs/*.h)
set(web-eid_RESOURCES src/web-eid.qrc src/translations/strings.qrc)

IF (MSVC)
	add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
	set(LIBS winscard ncrypt crypt32 cryptui Advapi32)
endif (MSVC)

if (APPLE)
	find_library(PCSC PCSC)
	find_library(ServiceManagement ServiceManagement)
	find_library(CoreFoundation CoreFoundation)
	find_library(AppKit AppKit)

	set (web-eid_SRCS src/dialogs/macosxui.mm ${web-eid_SRCS})
	set(LIBS ${CoreFoundation} ${PCSC} ${ServiceManagement} ${AppKit} dl)
endif (APPLE)

foreach(plugin ${Qt5Network_PLUGINS})
  get_target_property(_loc ${plugin} LOCATION)
  message("Plugin ${plugin} is at location ${_loc}")
endforeach()

include_directories(src src/dialogs)

add_definitions(-DVERSION="19.05.0-evidos")
add_definitions(-DGIT_REVISION="")

IF (MSVC)
	add_executable(web-eid WIN32 ${web-eid_SRCS} ${web-eid_RESOURCES})
elseif (APPLE)
	add_executable(web-eid MACOSX_BUNDLE ${web-eid_SRCS} ${web-eid_RESOURCES})
else()
	add_executable(web-eid ${web-eid_SRCS} ${web-eid_RESOURCES})
endif ()

target_link_libraries(web-eid
	Qt5::Widgets
	Qt5::Svg
	Qt5::Network
	Qt5::WebSockets
	Qt5::Concurrent
	${LIBS})

IF (MSVC)
	find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
	add_custom_command(TARGET web-eid POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} "Release/Web-eID.exe" $<TARGET_FILE:web-eid>)
endif (MSVC)