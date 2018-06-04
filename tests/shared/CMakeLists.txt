cmake_minimum_required(VERSION 3.5)
project(test-shared-library)

################################################################################
# System & Utilities
################################################################################
# Detect Architecture (Bitness)
math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

################################################################################
# Code
################################################################################

# File List
SET(THISPROJECT_SOURCES
	"${PROJECT_SOURCE_DIR}/lib.cpp"
	"${PROJECT_SOURCE_DIR}/lib.h"
)
SET(THISPROJECT_LIBRARIES
)

# Project
source_group("Data Files" FILES $THISPROJECT_DATA)

################################################################################
# Platform Dependencies
################################################################################
IF(WIN32)
	# Windows
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	
	LIST(APPEND THISPROJECT_SOURCES
	)
	LIST(APPEND THISPROJECT_DEPS
	)
ELSEIF(APPLE)
	# MacOSX

	LIST(APPEND THISPROJECT_SOURCES
	)
	LIST(APPEND THISPROJECT_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	# Linux
	
	LIST(APPEND THISPROJECT_SOURCES
	)
	LIST(APPEND THISPROJECT_DEPS
	)
ELSEIF("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
	# FreeBSD
	
	LIST(APPEND THISPROJECT_SOURCES
	)
	LIST(APPEND THISPROJECT_DEPS
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
ADD_LIBRARY(${PROJECT_NAME} STATIC
	${THISPROJECT_SOURCES}
)

# Linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	${THISPROJECT_LIBRARIES}
)
