cmake_minimum_required(VERSION 3.5)
project(streamlabs-ipc-node-server)

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

# Only allow Debug or Release builds.
SET(CMAKE_CONFIGURATION_TYPES			"Debug;Release")
SET(CMAKE_CXX_FLAGS_RELEASE				"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_C_FLAGS_RELEASE				"${CMAKE_C_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE		"${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE	"${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_RC_LINKER_FLAGS_RELEASE		"${CMAKE_RC_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE	"${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_STATIC_LINKER_FLAGS_RELEASE	"${CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO}")

################################################################################
# Code
################################################################################

# File List
SET(streamlabs-ipc-node-server_SOURCES
	"${PROJECT_SOURCE_DIR}/main.cpp"
)

################################################################################
# Platform Dependencies
################################################################################
IF(WIN32)
	# Windows
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
ENDIF()

################################################################################
# Building
################################################################################

# Includes
include_directories(
	"${lib-streamlabs-ipc_SOURCE_DIR}/include"
)

# Building
ADD_EXECUTABLE(${PROJECT_NAME}
	${streamlabs-ipc-node-server_SOURCES}
)

# Linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	${CMAKE_JS_LIB}
	lib-streamlabs-ipc
	winmm
)

INSTALL(TARGETS streamlabs-ipc-node-server DESTINATION "${PROJECT_SOURCE_DIR}/../distrib/")
INSTALL(FILES $<TARGET_PDB_FILE:streamlabs-ipc-node-server> DESTINATION "${PROJECT_SOURCE_DIR}/../distrib/" OPTIONAL)
