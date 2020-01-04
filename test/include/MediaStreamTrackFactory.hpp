#ifndef MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP
#define MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP

#include "api/media_stream_interface.h"

rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label);

rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label);

#endif
