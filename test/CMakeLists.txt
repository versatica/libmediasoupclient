set(
	SOURCE_FILES
	src/Device.test.cpp
	src/Handler.test.cpp
	src/PeerConnection.test.cpp
	src/RemoteSdp.test.cpp
	src/SdpUtils.test.cpp
	src/VcmCapturer.cpp
	src/VideoCapturer.cpp
	src/mediasoupclient.test.cpp
	src/ortc.test.cpp
	src/fakeParameters.cpp
	src/peerConnectionUtils.cpp
	src/scalabilityMode.test.cpp
	src/tests.cpp
	include/FakeTransportListener.hpp
	include/VcmCapturer.hpp
	include/VideoCapturer.hpp
	include/helpers.hpp
	include/fakeParameters.hpp
	include/peerConnectionUtils.hpp
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

if(UNIX)
	find_package(Threads REQUIRED)
	target_link_libraries(test_mediasoupclient PRIVATE Threads::Threads)
endif(UNIX)

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
target_link_libraries(test_mediasoupclient PRIVATE ${CMAKE_DL_LIBS})
