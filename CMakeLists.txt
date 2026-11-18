cmake_minimum_required(VERSION 3.14)

project(mediasoupclient LANGUAGES CXX)

include(FetchContent)

# Set version number.
set(mediasoupclient_VERSION_MAJOR 3)
set(mediasoupclient_VERSION_MINOR 5)
set(mediasoupclient_VERSION_PATCH 0)

# Configure a header file to pass the version.
configure_file (
	"${PROJECT_SOURCE_DIR}/version.hpp.in"
	"${PROJECT_SOURCE_DIR}/include/version.hpp"
	NEWLINE_STYLE LF
)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project options.
option(MEDIASOUPCLIENT_BUILD_TESTS "Build unit tests" OFF)
option(MEDIASOUPCLIENT_BUILD_DEMO  "Build demo app" OFF)
option(MEDIASOUPCLIENT_LOG_TRACE   "Enable MSC_LOG_TRACE (See Logger.hpp)" OFF)
option(MEDIASOUPCLIENT_LOG_DEV     "Enable MSC_LOG_DEV (See Logger.hpp)" OFF)

# Project configuration.
if(NOT LIBWEBRTC_INCLUDE_PATH)
	set(LIBWEBRTC_INCLUDE_PATH "" CACHE STRING "libwebrtc include path")
endif()
if(NOT LIBWEBRTC_BINARY_PATH)
	set(LIBWEBRTC_BINARY_PATH "" CACHE STRING "libwebrtc binary path")
endif()

if(NOT LIBWEBRTC_INCLUDE_PATH)
	message(FATAL_ERROR "LIBWEBRTC_INCLUDE_PATH not provided")
endif()

if(NOT LIBWEBRTC_BINARY_PATH)
	message(FATAL_ERROR "LIBWEBRTC_BINARY_PATH not provided")
endif()

message("\n=========== libmediasoupclient Build Configuration ===========\n")
message(STATUS "MEDIASOUPCLIENT_BUILD_TESTS : " ${MEDIASOUPCLIENT_BUILD_TESTS})
message(STATUS "MEDIASOUPCLIENT_BUILD_DEMO  : " ${MEDIASOUPCLIENT_BUILD_DEMO})
message(STATUS "MEDIASOUPCLIENT_LOG_TRACE   : " ${MEDIASOUPCLIENT_LOG_TRACE})
message(STATUS "MEDIASOUPCLIENT_LOG_DEV     : " ${MEDIASOUPCLIENT_LOG_DEV})
message(STATUS "LIBWEBRTC_INCLUDE_PATH      : " ${LIBWEBRTC_INCLUDE_PATH})
message(STATUS "LIBWEBRTC_BINARY_PATH       : " ${LIBWEBRTC_BINARY_PATH})
message("")

# GCC >= 4.9 is required due to std::regex use.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
		message(FATAL_ERROR "GCC version must be at least 4.9!")
	endif()
endif()

if (${MEDIASOUPCLIENT_BUILD_TESTS})
	add_subdirectory(test)
endif()

set(
	SOURCE_FILES
	src/Consumer.cpp
	src/DataConsumer.cpp
	src/DataProducer.cpp
	src/Device.cpp
	src/Handler.cpp
	src/Logger.cpp
	src/PeerConnection.cpp
	src/Producer.cpp
	src/Transport.cpp
	src/mediasoupclient.cpp
	src/ortc.cpp
	src/scalabilityMode.cpp
	src/sdp/MediaSection.cpp
	src/sdp/RemoteSdp.cpp
	src/sdp/Utils.cpp
)

# Create target.
add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

if(${MEDIASOUPCLIENT_LOG_TRACE})
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE MSC_LOG_TRACE=1
	)
endif()

if(${MEDIASOUPCLIENT_LOG_DEV})
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE MSC_LOG_DEV=1
	)
endif()

# Source Dependencies.

message(STATUS "\nFetching libsdptransform...\n")
FetchContent_Declare(
	libsdptransform
  	GIT_REPOSITORY https://github.com/ibc/libsdptransform.git
  	GIT_TAG 1.2.10
)
FetchContent_MakeAvailable(libsdptransform)

# Add some compile flags to our source files.
if(MSVC)
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS "/W3")
else()
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)
endif()

# Public (interface) headers of this library.
# NOTE: The installed tree needs two directories: "include" so that applications
# can use #include <mediasoupclient/mediasoupclient.hpp>, and
# "include/mediasoupclient" because the headers include each other with their
# folder (such as #include "sdp/MediaSection.hpp" in sdp/RemoteSdp.hpp), which
# keeps those includes unambiguous if another folder ever holds a header with
# the same name.
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/mediasoupclient>
)

# Public (interface) headers from dependencies.
target_include_directories(${PROJECT_NAME} PUBLIC
    ${libsdptransform_SOURCE_DIR}/include
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
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)

install(TARGETS mediasoupclient DESTINATION lib)

# NOTE: The whole include/ tree is installed (preserving the sdp/ subdirectory)
# rather than a hand written list of headers. mediasoupclient.hpp includes
# Device.hpp, which includes Transport.hpp, and so on, so installing just some of
# them gives the application a public header that does not compile.
install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/include/
	DESTINATION include/mediasoupclient
	FILES_MATCHING PATTERN "*.hpp"
)

if (${MEDIASOUPCLIENT_BUILD_DEMO})
	add_subdirectory(demo)
endif()
