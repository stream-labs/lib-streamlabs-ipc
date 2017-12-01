cmake_minimum_required(VERSION 3.5)
project(lib-streamlabs-ipc)

################################################################################
# System & Utilities
################################################################################
# Detect Libary Suffix
IF(WIN32)
	SET(libExtension ".dll")
ELSEIF(APPLE)
	SET(libExtension ".dylib")
ELSE()
    SET(libExtension ".so")
ENDIF()

# Detect Architecture (Bitness)
math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

# Find Third-Party Dependencies
## Protobuf
SET(Protobuf_USE_STATIC_LIBS ON)
SET(Protobuf_SRC_ROOT_FOLDER		"${PROJECT_SOURCE_DIR}/third-party/protobuf")
SET(Protobuf_INCLUDE_DIR			"${PROJECT_SOURCE_DIR}/third-party/protobuf/src")
SET(Protobuf_LIBRARY_DEBUG			"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Debug/libprotobufd.lib")
SET(Protobuf_LIBRARY_RELEASE		"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Release/libprotobuf.lib")
SET(Protobuf_LITE_LIBRARY_DEBUG		"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Debug/libprotobuf-lited.lib")
SET(Protobuf_LITE_LIBRARY_RELEASE	"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Release/libprotobuf-lite.lib")
SET(Protobuf_PROTOC_EXECUTABLE		"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Release/protoc.exe")
SET(Protobuf_PROTOC_LIBRARY_DEBUG	"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Debug/libprotocd.lib")
SET(Protobuf_PROTOC_LIBRARY_RELEASE	"${PROJECT_SOURCE_DIR}/third-party/protobuf/bin/Release/libprotoc.lib")
find_package(Protobuf REQUIRED)

################################################################################
# Code
################################################################################

# File List
SET(lib-streamlabs-ipc_SOURCES
	"${PROJECT_SOURCE_DIR}/source/ipc.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-class.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-class.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-client.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-client.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-function.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-function.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-message.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-message.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-server.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-server.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-server-instance.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-server-instance.hpp"
	"${PROJECT_SOURCE_DIR}/source/ipc-value.cpp"
	"${PROJECT_SOURCE_DIR}/include/ipc-value.hpp"
	"${PROJECT_SOURCE_DIR}/source/os-namedsocket.cpp"
	"${PROJECT_SOURCE_DIR}/include/os-namedsocket.hpp"
	"${PROJECT_SOURCE_DIR}/include/util.h"
)
SET(Protobuf_IMPORT_DIRS
	"${PROJECT_SOURCE_DIR}/proto"
)
SET(lib-streamlabs-ipc_PROTOBUF
	"${PROJECT_SOURCE_DIR}/proto/example.proto"
	"${PROJECT_SOURCE_DIR}/proto/ipc.proto"
)
SET(lib-streamlabs-ipc_DATA
	"${PROJECT_SOURCE_DIR}/index.js"
	"${PROJECT_SOURCE_DIR}/README.md"
	"${PROJECT_SOURCE_DIR}/LICENSE"
)
SET(lib-streamlabs-ipc_DEPS
	"${PROJECT_SOURCE_DIR}/LICENSE"
	"${PROJECT_SOURCE_DIR}/index.js"
	"${PROJECT_SOURCE_DIR}/package.json"
)
SET(lib-streamlabs-ipc_LIBRARIES
)

# Project
source_group("Data Files" FILES $lib-streamlabs-ipc_DATA)

################################################################################
# Platform Dependencies
################################################################################
IF(WIN32)
	# Windows
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	
	LIST(APPEND lib-streamlabs-ipc_SOURCES
		"${PROJECT_SOURCE_DIR}/include/os-namedsocket-win.hpp"
		"${PROJECT_SOURCE_DIR}/source/os-namedsocket-win.cpp"
	)
	LIST(APPEND lib-streamlabs-ipc_DEPS
	)
ELSEIF(APPLE)
	# MacOSX

	LIST(APPEND lib-streamlabs-ipc_SOURCES
		"${PROJECT_SOURCE_DIR}/source/os-namedsocket-mac.cpp"
	)
	LIST(APPEND lib-streamlabs-ipc_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	# Linux
	
	LIST(APPEND lib-streamlabs-ipc_SOURCES
		"${PROJECT_SOURCE_DIR}/source/os-namedsocket-linux.cpp"
	)
	LIST(APPEND lib-streamlabs-ipc_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
	# FreeBSD
	
	LIST(APPEND lib-streamlabs-ipc_SOURCES
		"${PROJECT_SOURCE_DIR}/source/os-namedsocket-freebsd.cpp"
	)
	LIST(APPEND lib-streamlabs-ipc_DEPS
	)
ENDIF()

################################################################################
# Building
################################################################################
# Protobuf
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${lib-streamlabs-ipc_PROTOBUF})

# Directories
INCLUDE_DIRECTORIES(
	${PROJECT_SOURCE_DIR}/source
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/third-party/boost/
	${Protobuf_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
)
LINK_DIRECTORIES(
	${Boost_LIBRARY_DIRS}
)

# Building
ADD_LIBRARY(${PROJECT_NAME} STATIC
	${lib-streamlabs-ipc_SOURCES}
	${PROTO_SRCS} ${PROTO_HDRS}
)

# Linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	${lib-streamlabs-ipc_LIBRARIES}
	${Protobuf_LIBRARIES}
	${Boost_LIBRARIES}
)