cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# Unit tests - logic + WebRTC integration tests that do not require media tracks.
# Device.test.cpp and PeerConnection.test.cpp are here because they exercise
# WebRTC/mediasoupclient APIs without needing any media capture.
set(
	SOURCE_FILES
	# Tests
	src/Device.test.cpp
	src/Handler.test.cpp
	src/mediasoupclient.test.cpp
	src/ortc.test.cpp
	src/PeerConnection.test.cpp
	src/RemoteSdp.test.cpp
	src/scalabilityMode.test.cpp
	src/SdpUtils.test.cpp
	src/smoke.test.cpp
	# Support
	src/fakeParameters.cpp
	src/MediaStreamTrackFactory.cpp
	src/tests.cpp
	# Headers
	include/fakeParameters.hpp
	include/FakeTransportListener.hpp
	include/helpers.hpp
	include/MediaStreamTrackFactory.hpp
)

# Create unit test target
add_executable(test ${SOURCE_FILES})

# Source deps
message(STATUS "\nFetching catch2...\n")
FetchContent_Declare(
	catch2
  	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  	GIT_TAG v2.13.10
)
FetchContent_MakeAvailable(catch2)

# Configure unit tests
target_include_directories(test PRIVATE
	include
	${mediasoupclient_SOURCE_DIR}/include
	${catch2_SOURCE_DIR}/single_include/catch2
)

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)

	target_link_libraries(test PRIVATE
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
	)

	set_target_properties(test PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
	)
endif(APPLE)

if(UNIX)
	find_package(Threads REQUIRED)
	target_link_libraries(test PRIVATE Threads::Threads)
endif(UNIX)

target_compile_definitions(test PUBLIC
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	# Required to match the release-mode libwebrtc.a: without it, inline methods
	# in WebRTC headers (e.g. VideoTrackSource::state) expand RTC_DCHECK_RUN_ON
	# into code that references SequenceCheckerImpl::ExpectationToString, a
	# debug-only symbol absent from a release build.
	NDEBUG
)

# Source Dependencies.
add_subdirectory(deps/libwebrtc "${CMAKE_CURRENT_BINARY_DIR}/libwebrtc")

# Unit tests
target_link_libraries(test PRIVATE mediasoupclient)
target_link_libraries(test PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(test PUBLIC
	webrtc
	${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
)
