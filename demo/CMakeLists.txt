cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# ixwebsocket — WebSocket client with TLS support
message(STATUS "\nFetching ixwebsocket...\n")
FetchContent_Declare(
	ixwebsocket
	GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
	GIT_TAG        v11.4.5
)
set(USE_TLS ON CACHE BOOL "Enable TLS in ixwebsocket" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# CLI11 — command-line argument parsing
message(STATUS "\nFetching CLI11...\n")
FetchContent_Declare(
	cli11
	GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
	GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(cli11)

add_executable(mediasoupclient_demo
	include/AVFCaptureVideoSource.hpp
	include/ProtooClient.hpp
	include/RoomClient.hpp
	src/AVFCaptureVideoSource.mm
	src/ProtooClient.cpp
	src/RoomClient.cpp
	src/main.cpp
)

target_compile_definitions(mediasoupclient_demo PRIVATE
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	NDEBUG
)

target_include_directories(mediasoupclient_demo PRIVATE
	include
	${mediasoupclient_SOURCE_DIR}/include
	${LIBWEBRTC_INCLUDE_PATH}/third_party/libyuv/include
)

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(AVFOUNDATION AVFoundation)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_MEDIA CoreMedia)
	find_library(CORE_VIDEO CoreVideo)
	find_library(SECURITY Security)

	target_link_libraries(mediasoupclient_demo PRIVATE
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${AVFOUNDATION}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
		${CORE_MEDIA}
		${CORE_VIDEO}
		${SECURITY}
	)

	set_target_properties(mediasoupclient_demo PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
	)
endif()

if(UNIX)
	find_package(Threads REQUIRED)
	target_link_libraries(mediasoupclient_demo PRIVATE Threads::Threads)
endif()

target_link_libraries(mediasoupclient_demo PRIVATE
	mediasoupclient
	ixwebsocket
	CLI11::CLI11
	${CMAKE_DL_LIBS}
)

# libwebrtc is added after add_subdirectory in the parent scope,
# so we link it explicitly here.
target_link_libraries(mediasoupclient_demo PUBLIC
	${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
)
