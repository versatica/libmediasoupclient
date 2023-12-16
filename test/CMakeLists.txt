cmake_minimum_required(VERSION 3.14)

include(FetchContent)

set(
	SOURCE_FILES
	src/Device.test.cpp
	src/Handler.test.cpp
	src/PeerConnection.test.cpp
	src/RemoteSdp.test.cpp
	src/SdpUtils.test.cpp
	src/mediasoupclient.test.cpp
	src/MediaStreamTrackFactory.cpp
	src/ortc.test.cpp
	src/fakeParameters.cpp
	src/scalabilityMode.test.cpp
	src/tests.cpp
	include/FakeTransportListener.hpp
	include/MediaStreamTrackFactory.hpp
	include/helpers.hpp
	include/fakeParameters.hpp
)

# Create target.
add_executable(test_mediasoupclient ${SOURCE_FILES})

# Source deps
message(STATUS "\nFetching catch2...\n")
FetchContent_Declare(
	catch2
  	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  	GIT_TAG v2.13.10
)
FetchContent_MakeAvailable(catch2)

# Private (implementation) header files.
target_include_directories(test_mediasoupclient PRIVATE
	include
	${mediasoupclient_SOURCE_DIR}/include
	${catch2_SOURCE_DIR}/single_include/catch2
)

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)

	target_link_libraries(test_mediasoupclient PRIVATE
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
	)

	# Bundle it.
	set_target_properties(test_mediasoupclient PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
	)
endif(APPLE)

if(UNIX)
	find_package(Threads REQUIRED)
	target_link_libraries(test_mediasoupclient PRIVATE Threads::Threads)
endif(UNIX)

target_compile_definitions(test_mediasoupclient PUBLIC
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
target_link_libraries(test_mediasoupclient PRIVATE ${CMAKE_DL_LIBS})

# Source Dependencies.
add_subdirectory(deps/libwebrtc "${CMAKE_CURRENT_BINARY_DIR}/libwebrtc")

# Public (interface) dependencies.
target_link_libraries(test_mediasoupclient PUBLIC
	webrtc
)
