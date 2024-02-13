cmake_minimum_required(VERSION 3.14)

project(mediasoupclient LANGUAGES CXX)

include(FetchContent)

# Set version number.
set(mediasoupclient_VERSION_MAJOR 3)
set(mediasoupclient_VERSION_MINOR 4)
set(mediasoupclient_VERSION_PATCH 2)

# Configure a header file to pass the version.
configure_file (
	"${PROJECT_SOURCE_DIR}/version.hpp.in"
	"${PROJECT_SOURCE_DIR}/include/version.hpp"
	NEWLINE_STYLE LF
)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project options.
option(MEDIASOUPCLIENT_BUILD_TESTS "Build unit tests" OFF)
option(MEDIASOUPCLIENT_LOG_TRACE   "Enable MSC_LOG_TRACE (See Logger.hpp)" OFF)
option(MEDIASOUPCLIENT_LOG_DEV     "Enable MSC_LOG_DEV (See Logger.hpp)" OFF)

# Project configuration.
set(LIBWEBRTC_INCLUDE_PATH "" CACHE STRING "libwebrtc include path")
set(LIBWEBRTC_BINARY_PATH "" CACHE STRING "libwebrtc binary path")

if(NOT LIBWEBRTC_INCLUDE_PATH)
	message(FATAL_ERROR "LIBWEBRTC_INCLUDE_PATH not provided")
endif()

if(NOT LIBWEBRTC_BINARY_PATH)
	message(FATAL_ERROR "LIBWEBRTC_BINARY_PATH not provided")
endif()

message("\n=========== libmediasoupclient Build Configuration ===========\n")
message(STATUS "MEDIASOUPCLIENT_BUILD_TESTS : " ${MEDIASOUPCLIENT_BUILD_TESTS})
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

set(HEADER_FILES
	include/mediasoupclient.hpp
)

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
	include/Consumer.hpp
	include/Device.hpp
	include/Handler.hpp
	include/Logger.hpp
	include/MediaSoupClientErrors.hpp
	include/PeerConnection.hpp
	include/Producer.hpp
	include/Transport.hpp
	include/mediasoupclient.hpp
	include/ortc.hpp
	include/scalabilityMode.hpp
	include/sdp/MediaSection.hpp
	include/sdp/RemoteSdp.hpp
	include/sdp/Utils.hpp
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

# Private (implementation) header files.
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

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
install(FILES ${HEADER_FILES} DESTINATION include/mediasoupclient)
