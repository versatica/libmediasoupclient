include_directories(${mediasoup-client_SOURCE_DIR}/include)
include_directories(include)

set(
	SOURCE_FILES
	Device.test.cpp
	FakeVideoCapturer.cpp
	Handler.test.cpp
	PeerConnection.test.cpp
	Transport.test.cpp
	data/Parameters.cpp
	ortc.test.cpp
	sdp/RemoteSdp.test.cpp
	sdp/Utils.test.cpp
	tests.cpp
)

add_executable(test_mediasoup-client ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(test_mediasoup-client mediasoup-client)
