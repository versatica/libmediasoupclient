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
