set(
	SOURCE_FILES
	Device.test.cpp
	Handler.test.cpp
	PeerConnection.test.cpp
	data/Parameters.cpp
	mediasoupclient.test.cpp
	ortc.test.cpp
	peerConnectionUtils.cpp
	scalabilityMode.test.cpp
	sdp/RemoteSdp.test.cpp
	sdp/Utils.test.cpp
	tests.cpp
	VcmCapturer.cpp
	VideoCapturer.cpp
)

# Create taget.
add_executable(test_mediasoupclient ${SOURCE_FILES})

if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(AV_FOUNDATION AVFoundation)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_MEDIA CoreMedia)
	find_library(CORE_VIDEO CoreVideo)
	find_library(CORE_SERVICES CoreServices)

	target_link_libraries(test_mediasoupclient PRIVATE
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${AV_FOUNDATION}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
		${CORE_MEDIA}
		${CORE_VIDEO}
		${CORE_SERVICES}
	)

	# Bundle it.
	set_target_properties(test_mediasoupclient PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
	)
endif(APPLE)

# Private (implementation) header files.
target_include_directories(test_mediasoupclient PRIVATE
	${mediasoupclient_SOURCE_DIR}/include
	include
)

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
