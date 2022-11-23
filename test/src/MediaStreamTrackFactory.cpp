#define MSC_CLASS "MediaStreamTrackFactory"

#include "pc/test/fake_video_track_source.h"
#include "PeerConnection.hpp"

using namespace mediasoupclient;

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	cricket::AudioOptions options;
	options.highpass_filter = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source = PeerConnection::DefaultFactory()->CreateAudioSource(options);

	return PeerConnection::DefaultFactory()->CreateAudioTrack(label, source.get());
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	return PeerConnection::DefaultFactory()->CreateVideoTrack(label, webrtc::FakeVideoTrackSource::Create().get());
}
