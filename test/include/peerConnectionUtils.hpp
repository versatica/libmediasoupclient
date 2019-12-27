#ifndef MSC_TEST_PEERCONNECTION_UTILS_HPP
#define MSC_TEST_PEERCONNECTION_UTILS_HPP

#include <api/create_peerconnection_factory.h>

// Create PeerConnection factory.
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createPeerConnectionFactory();

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label);

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label);

#endif
