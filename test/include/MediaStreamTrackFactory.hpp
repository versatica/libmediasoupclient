#ifndef MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP
#define MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP

#include "api/media_stream_interface.h"
// NOTE: Included for the mediasoupclient declarations it re-exports (such as
// PeerConnection::Options), not used directly.
#include "mediasoupclient.hpp" // IWYU pragma: keep
#include "rtc_base/thread.h"
#include <memory>

class MediaStreamTrackFactory
{
public:
	static MediaStreamTrackFactory& GetInstance()
	{
		static MediaStreamTrackFactory instance;
		return instance;
	}

	MediaStreamTrackFactory(const MediaStreamTrackFactory&)            = delete;
	MediaStreamTrackFactory& operator=(const MediaStreamTrackFactory&) = delete;

	void Create();

	void Destroy();

	webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
	mediasoupclient::PeerConnection::Options peerConnectionOptions;
	webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource;

private:
	MediaStreamTrackFactory()
	{
		Create();
	}
	~MediaStreamTrackFactory()
	{
		Destroy();
	}

	std::unique_ptr<webrtc::Thread> networkThread;
	std::unique_ptr<webrtc::Thread> workerThread;
	std::unique_ptr<webrtc::Thread> signalingThread;
};

webrtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& id);

webrtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& id);

#endif
