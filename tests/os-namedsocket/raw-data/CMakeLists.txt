cmake_minimum_required(VERSION 3.5)
project(os-namedsocket_test_raw-data)

ADD_SUBDIRECTORY("../../../" "lib-streamlabs-ipc")

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

################################################################################
# Code
################################################################################

# File List
SET(ipc-test_SOURCES
	"${PROJECT_SOURCE_DIR}/test.cpp"
)
SET(ipc-test_LIBRARIES
)

# Project
source_group("Data Files" FILES $ipc-test_DATA)

################################################################################
# Platform Dependencies
################################################################################
IF(WIN32)
	# Windows
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	
	LIST(APPEND ipc-test_SOURCES
	)
	LIST(APPEND ipc-test_DEPS
	)
ELSEIF(APPLE)
	# MacOSX

	LIST(APPEND ipc-test_SOURCES
	)
	LIST(APPEND ipc-test_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	# Linux
	
	LIST(APPEND ipc-test_SOURCES
	)
	LIST(APPEND ipc-test_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
	# FreeBSD
	
	LIST(APPEND ipc-test_SOURCES
	)
	LIST(APPEND ipc-test_DEPS
	)
ENDIF()

################################################################################
# Building
################################################################################
# Includes
include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}
	${lib-streamlabs-ipc_SOURCE_DIR}/include
)

# Building
ADD_EXECUTABLE(${PROJECT_NAME}
	${ipc-test_SOURCES}
)

# Linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	lib-streamlabs-ipc
	${ipc-test_LIBRARIES}
)
