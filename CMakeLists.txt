################################################################################
# CMake Initialize
################################################################################
cmake_minimum_required(VERSION 3.16)

project(lib-streamlabs-ipc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} "${PROJECT_SOURCE_DIR}/cmake")

# DownloadProject
INCLUDE(DownloadProject)

# Detect Architecture (Bitness)
math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

# Windows
IF(WIN32)
	# Detect Visual Studio Version
	STRING(REGEX REPLACE "^Visual Studio [0-9]+ ([0-9]+)(| Win64)$" "\\1" VS_YEAR "${CMAKE_GENERATOR}")
ENDIF()

################################################################################
# Settings
################################################################################
OPTION(lib-streamlabs-ipc_BUILD_TESTS "Build lib-streamlabs-ipc Tests" OFF)

################################################################################
# Code
################################################################################

# File List
SET(lib-streamlabs-ipc_SOURCES
	"${PROJECT_SOURCE_DIR}/source/ipc.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-class.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-class.hpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-client.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-function.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-function.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-server.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-server.hpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-server-instance.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-value.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-value.hpp"
	"${PROJECT_SOURCE_DIR}/include/util.h"
	"${PROJECT_SOURCE_DIR}/include/waitable.hpp"
	"${PROJECT_SOURCE_DIR}/include/tags.hpp"
	"${PROJECT_SOURCE_DIR}/include/semaphore.hpp"
	"${PROJECT_SOURCE_DIR}/include/error.hpp"
	"${PROJECT_SOURCE_DIR}/include/async_op.hpp"
	"${PROJECT_SOURCE_DIR}/source/async_op.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-socket.hpp"
)
IF (WIN32)
	SET(lib-streamlabs-ipc_SOURCES_WINDOWS
		"${PROJECT_SOURCE_DIR}/source/windows/utility.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/utility.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/semaphore.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/semaphore.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/overlapped.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/overlapped.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/async_request.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/async_request.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/waitable.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-client-win.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-client-win.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-server-instance-win.cpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-server-instance-win.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-socket-win.hpp"
		"${PROJECT_SOURCE_DIR}/source/windows/ipc-socket-win.cpp"
	)
ELSEIF(APPLE)
    SET(lib-streamlabs-ipc_SOURCES_APPLE
		"${PROJECT_SOURCE_DIR}/source/apple/semaphore.hpp"
		"${PROJECT_SOURCE_DIR}/source/apple/semaphore.cpp"
		"${PROJECT_SOURCE_DIR}/source/apple/async_request.hpp"
		"${PROJECT_SOURCE_DIR}/source/apple/async_request.cpp"
		"${PROJECT_SOURCE_DIR}/source/apple/waitable.cpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-client-osx.cpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-client-osx.hpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-server-instance-osx.cpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-server-instance-osx.hpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-socket-osx.hpp"
		"${PROJECT_SOURCE_DIR}/source/apple/ipc-socket-osx.cpp"
    )
ENDIF()
SET(Protobuf_IMPORT_DIRS
	"${PROJECT_SOURCE_DIR}/proto"
)
SET(lib-streamlabs-ipc_PROTOBUF
	"${PROJECT_SOURCE_DIR}/proto/ipc.proto"
)
SET(lib-streamlabs-ipc_DATA
	"${PROJECT_SOURCE_DIR}/README.md"
	"${PROJECT_SOURCE_DIR}/LICENSE"
)
SET(lib-streamlabs-ipc_DEPS
	"${PROJECT_SOURCE_DIR}/LICENSE"
)

# Project
source_group("Data Files" FILES $lib-streamlabs-ipc_DATA)

################################################################################
# Platform Code
################################################################################
IF(WIN32)
	# Windows
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	# windows.h
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	add_definitions(-DNOGPICAPMASKS)
	add_definitions(-DNOVIRTUALKEYCODES)
	add_definitions(-DNOWINMESSAGES)
	add_definitions(-DNOWINSTYLES)
	add_definitions(-DNOSYSMETRICS)
	add_definitions(-DNOMENUS)
	add_definitions(-DNOICONS)
	add_definitions(-DNOKEYSTATES)
	add_definitions(-DNOSYSCOMMANDS)
	add_definitions(-DNORASTEROPS)
	add_definitions(-DNOSHOWWINDOW)
	add_definitions(-DNOATOM)
	add_definitions(-DNOCLIPBOARD)
	add_definitions(-DNOCOLOR)
	add_definitions(-DNOCTLMGR)
	add_definitions(-DNODRAWTEXT)
	add_definitions(-DNOGDI)
	add_definitions(-DNOKERNEL)
	#add_definitions(-DNOUSER)
	add_definitions(-DNONLS)
	add_definitions(-DNOMB)
	add_definitions(-DNOMEMMGR)
	add_definitions(-DNOMETAFILE)
	add_definitions(-DNOMINMAX)
	#add_definitions(-DNOMSG)
	add_definitions(-DNOOPENFILE)
	add_definitions(-DNOSCROLL)
	add_definitions(-DNOSERVICE)
	add_definitions(-DNOSOUND)
	add_definitions(-DNOTEXTMETRIC)
	add_definitions(-DNOWH)
	add_definitions(-DNOWINOFFSETS)
	add_definitions(-DNOCOMM)
	add_definitions(-DNOKANJI)
	add_definitions(-DNOHELP)
	add_definitions(-DNOPROFILER)
	add_definitions(-DNODEFERWINDOWPOS)
	add_definitions(-DNOMCX)
	
	LIST(
		APPEND
		lib-streamlabs-ipc_SOURCES
		${lib-streamlabs-ipc_SOURCES_WINDOWS}
	)
ELSEIF(APPLE)
	# MacOSX
	LIST(
		APPEND
		lib-streamlabs-ipc_SOURCES
		${lib-streamlabs-ipc_SOURCES_APPLE}
	)
ENDIF()

################################################################################
# Building
################################################################################

# Directories
INCLUDE_DIRECTORIES(
	${PROJECT_SOURCE_DIR}/source
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/third-party/boost/
	${PROJECT_BINARY_DIR}
)
LINK_DIRECTORIES(
)

# Building
ADD_LIBRARY(${PROJECT_NAME} STATIC
	${lib-streamlabs-ipc_SOURCES}
)

# Linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	${lib-streamlabs-ipc_LIBRARIES}
)

################################################################################
# Others
################################################################################
IF(lib-streamlabs-ipc_BUILD_TESTS)
	ADD_SUBDIRECTORY(tests/shared)
	ADD_SUBDIRECTORY(tests/ipc/simple-multi-client)
	ADD_SUBDIRECTORY(tests/ipc/synchronous-call)
ENDIF(lib-streamlabs-ipc_BUILD_TESTS)
