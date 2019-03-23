cmake_minimum_required(VERSION 3.5)

project(mediasoupclient LANGUAGES CXX)

# Set version number.
set(mediasoupclient_VERSION_MAJOR 0)
set(mediasoupclient_VERSION_MINOR 0)
set(mediasoupclient_VERSION_PATCH 1)

# Configure a header file to pass the version.
configure_file (
  "${PROJECT_SOURCE_DIR}/version.hpp.in"
  "${PROJECT_SOURCE_DIR}/include/version.hpp"
)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project options.
option(BUILD_TESTS "Build unit tests" ON)
option(LOG_TRACE   "Enable MSC_LOG_TRACE (See Logger.hpp)" OFF)
option(LOG_DEV     "Enable MSC_LOG_DEV (See Logger.hpp)" OFF)

# Project configuration.
set(LIBWEBRTC_INCLUDE_PATH "" CACHE STRING "libwebrtc include path")
set(LIBWEBRTC_BINARY_PATH "" CACHE STRING "libwebrtc binary path")

if(NOT LIBWEBRTC_INCLUDE_PATH)
	message(FATAL_ERROR "LIBWEBRTC_INCLUDE_PATH not provided")
endif()

if(NOT LIBWEBRTC_BINARY_PATH)
	message(FATAL_ERROR "LIBWEBRTC_BINARY_PATH not provided")
endif()

message ("\n=========== libmediasoupclient Build Configuration =============\n")
message (STATUS "BUILD_TESTS            = " ${BUILD_TESTS})
message (STATUS "LOG_TRACE              = " ${LOG_TRACE})
message (STATUS "LOG_DEV                = " ${LOG_DEV})
message (STATUS "LIBWEBRTC_INCLUDE_PATH = " ${LIBWEBRTC_INCLUDE_PATH})
message (STATUS "LIBWEBRTC_BINARY_PATH  = " ${LIBWEBRTC_BINARY_PATH})
message ("")

if (${BUILD_TESTS})
	add_subdirectory(test)
endif()

set(HEADER_FILES
	include/mediasoupclient.hpp
)

# Add some compile flags to our source files.
if(MSVC)
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS "/W3 /WX")
else()
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)
endif()

# Create target.
add_library(${PROJECT_NAME} STATIC ${SOURCES})

if(${LOG_TRACE})
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE MSC_LOG_TRACE=1
	)
endif()

if(${LOG_DEV})
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE MSC_LOG_DEV=1
	)
endif()

# Source Dependencies.
add_subdirectory(deps/libsdptransform "${CMAKE_CURRENT_BINARY_DIR}/libsdptransform")

target_sources(${PROJECT_NAME} PRIVATE
	src/Consumer.cpp
	src/Device.cpp
	src/Handler.cpp
	src/Logger.cpp
	src/mediasoupclient.cpp
	src/PeerConnection.cpp
	src/Producer.cpp
	src/Transport.cpp
	src/ortc.cpp
	src/sdp/MediaSection.cpp
	src/sdp/RemoteSdp.cpp
	src/sdp/Utils.cpp
)

# Private (implementation) header files.
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Public (interface) headers from dependencies.
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/deps/libsdptransform/include"
	"${LIBWEBRTC_INCLUDE_PATH}"
	"${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)

# Public (interface) dependencies.
target_link_libraries(${PROJECT_NAME} PUBLIC
	sdptransform
	${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
)

# Compile definitions for libwebrtc.
target_compile_definitions(${PROJECT_NAME} PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)

install(TARGETS mediasoupclient DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/mediasoupclient)
