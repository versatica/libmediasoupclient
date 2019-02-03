cmake_minimum_required(VERSION 3.5)

project(mediasoupclient LANGUAGES CXX)

# Set version number.
set(mediasoupclient_VERSION_MAJOR 0)
set(mediasoupclient_VERSION_MINOR 1)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project options.
option(BUILD_TESTS "Build unit tests" ON)
option(LOG_TRACE   "Enable MSC_LOG_TRACE (See Logger.hpp)" OFF)

if (${BUILD_TESTS})
	subdirs(test)
endif()

if(${LOG_TRACE})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSC_LOG_TRACE")
endif()

set(SOURCE_FILES
	src/Consumer.cpp
	src/Device.cpp
	src/Handler.cpp
	src/Logger.cpp
	src/PeerConnection.cpp
	src/Producer.cpp
	src/Transport.cpp
	src/ortc.cpp
	src/sdp/RemoteSdp.cpp
	src/sdp/Utils.cpp
)

set(HEADER_FILES
	include/mediasoupclient.hpp
)

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
	find_library(COREAUDIO_LIBRARY CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_SERVICES CoreServices)

	set(EXTRA_LIBS
		${APPLICATION_SERVICES}
		${AUDIOTOOLBOX_LIBRARY}
		${COREAUDIO_LIBRARY}
		${CORE_FOUNDATION}
		${CORE_SERVICES}
	)
endif(APPLE)

# Add some compile flags to our source files.
if (MSVC)
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS /W3 /WX)
else ()
	set_source_files_properties(${SOURCE_FILES}
		PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)
endif()

# Create target.
add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

# Source Dependencies.
add_subdirectory(deps/libsdptransform "${CMAKE_CURRENT_BINARY_DIR}/libsdptransform")

# Private (implementation) header files.
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Public (interface) headers from dependencies.
target_include_directories(${PROJECT_NAME} PUBLIC
	"${PROJECT_SOURCE_DIR}/deps/libsdptransform/include"
	"${PROJECT_SOURCE_DIR}/deps/libwebrtc/include"
)

# Public (interface) dependencies.
target_link_libraries(${PROJECT_NAME} PUBLIC
	sdptransform
	${PROJECT_SOURCE_DIR}/deps/libwebrtc/lib/libwebrtc.a
	${EXTRA_LIBS}
)

# Compile definitions for libwebrtc.
target_compile_definitions(${PROJECT_NAME} PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)

install(TARGETS mediasoupclient DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/mediasoupclient)
