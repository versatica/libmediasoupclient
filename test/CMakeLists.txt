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

# Private (implementation) header files.
target_include_directories(test_mediasoupclient PRIVATE
	include
	${mediasoupclient_SOURCE_DIR}/include
	${mediasoupclient_SOURCE_DIR}/deps/catch/single_include/catch2
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

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
target_link_libraries(test_mediasoupclient PRIVATE ${CMAKE_DL_LIBS})

# Create a library with required webrtc extra code and link against it.
file(GLOB_RECURSE FAKE_AUDIO_CAPTURE
	${LIBWEBRTC_BINARY_PATH}/pc/pc_test_utils/fake_audio_capture_module.o
)

set(WEBRTC_EXTRA_OBJS ${FAKE_AUDIO_CAPTURE})

add_library (webrtc_extra STATIC ${WEBRTC_EXTRA_OBJS})
set_target_properties(webrtc_extra PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (test_mediasoupclient PUBLIC webrtc_extra)
