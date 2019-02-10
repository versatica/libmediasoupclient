set(
	SOURCE_FILES
	Device.test.cpp
	FakeVideoCapturer.cpp
	Handler.test.cpp
	PeerConnection.test.cpp
	data/Parameters.cpp
	mediasoupclient.test.cpp
	ortc.test.cpp
	peerConnectionUtils.cpp
	sdp/RemoteSdp.test.cpp
	sdp/Utils.test.cpp
	tests.cpp
)

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_SERVICES CoreServices)

	set(LIBWEBRTC_EXTRA_LIBS
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
		PARENT_SCOPE
	)
endif(APPLE)

# Create taget.
add_executable(test_mediasoupclient ${SOURCE_FILES})

# Private (implementation) header files.
target_include_directories(test_mediasoupclient PRIVATE
	${mediasoupclient_SOURCE_DIR}/include
	include
)

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
