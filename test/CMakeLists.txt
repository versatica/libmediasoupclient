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

# Create taget.
add_executable(test_mediasoupclient ${SOURCE_FILES})

# Private (implementation) header files.
target_include_directories(test_mediasoupclient PRIVATE
	${mediasoupclient_SOURCE_DIR}/include
	include
)

# Private dependencies.
target_link_libraries(test_mediasoupclient PRIVATE mediasoupclient)
