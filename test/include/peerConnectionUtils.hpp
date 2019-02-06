#ifndef PEERCONNECTION_UTILS_HPP
#define PEERCONNECTION_UTILS_HPP

#include "api/create_peerconnection_factory.h"

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label);

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label);

#endif
