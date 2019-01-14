include_directories(${mediasoupclient_SOURCE_DIR}/include)
include_directories(include)

set(
	SOURCE_FILES
	Device.test.cpp
	FakeVideoCapturer.cpp
	Handler.test.cpp
	PeerConnection.test.cpp
	data/Parameters.cpp
	mediasoupclient.test.cpp
	ortc.test.cpp
	sdp/RemoteSdp.test.cpp
	sdp/Utils.test.cpp
	tests.cpp
)

add_executable(test_mediasoupclient ${SOURCE_FILES})

target_link_libraries(test_mediasoupclient mediasoupclient)
