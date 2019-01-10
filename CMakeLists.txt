cmake_minimum_required(VERSION 3.3)

project(mediasoup-client VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++11")

# Disable rtti in order to avoi issues inheriting from pure virtual classes in libwebrtc.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

# Enable MSC_LOG_TRACE (See Logger.hpp).
if(${MSC_LOG_TRACE})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSC_LOG_TRACE")
endif()

# Enable MSC_LOG_DEV in all files (See Logger.hpp). Useful for clang-tidy.
if(${MSC_LOG_DEV})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSC_LOG_DEV")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_POSIX")

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
	find_library(COREAUDIO_LIBRARY CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_SERVICES CoreServices)

	set(EXTRA_LIBS ${APPLICATION_SERVICES} ${AUDIOTOOLBOX_LIBRARY} ${COREAUDIO_LIBRARY} ${CORE_FOUNDATION} ${CORE_SERVICES})

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_MAC")
endif(APPLE)

subdirs(test)

include_directories(${mediasoup-client_SOURCE_DIR}/include)

set(
	SOURCE_FILES
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

#set(
#	HEADER_FILES
#)

add_library(mediasoup-client STATIC ${SOURCE_FILES} ${HEADER_FILES})

# Add dependencies.
include_directories("${CMAKE_SOURCE_DIR}/deps/libwebrtc/include")
include_directories("${CMAKE_SOURCE_DIR}/deps/libwebrtc/include/webrtc")
include_directories("${CMAKE_SOURCE_DIR}/deps/libsdptransform/include")

target_link_libraries(${PROJECT_NAME}
	${CMAKE_SOURCE_DIR}/deps/libwebrtc/lib/libwebrtc.a
	${CMAKE_SOURCE_DIR}/deps/libsdptransform/build/libsdptransform.a
	${EXTRA_LIBS}
)

install(TARGETS mediasoup-client DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/mediasoup-client)
