#ifndef MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP
#define MSC_TEST_MEDIA_STREAM_TRACK_FACTORY_HPP

#include "api/media_stream_interface.h"
#include "mediasoupclient.hpp"
#include "rtc_base/thread.h"
#include <memory>

class MediaStreamTrackFactory
{
public:
	static MediaStreamTrackFactory& getInstance()
	{
		static MediaStreamTrackFactory instance;
		return instance;
	}

	MediaStreamTrackFactory(const MediaStreamTrackFactory&)            = delete;
	MediaStreamTrackFactory& operator=(const MediaStreamTrackFactory&) = delete;

	void Create();
	void Destroy();

	webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> Factory;
	mediasoupclient::PeerConnection::Options PeerConnectionOptions;
	webrtc::scoped_refptr<webrtc::AudioSourceInterface> AudioSource;

private:
	MediaStreamTrackFactory()
	{
		Create();
	}
	~MediaStreamTrackFactory()
	{
		Destroy();
	}

	std::unique_ptr<webrtc::Thread> NetworkThread;
	std::unique_ptr<webrtc::Thread> WorkerThread;
	std::unique_ptr<webrtc::Thread> SignalingThread;
};

webrtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& id);
webrtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& id);

#endif
