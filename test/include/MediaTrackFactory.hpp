#ifndef MSC_TEST_MEDIA_TRACK_FACTORY_HPP
#define MSC_TEST_MEDIA_TRACK_FACTORY_HPP

#include "api/create_peerconnection_factory.h"
#include <memory>

class MediaTrackFactory
{
	public:
		MediaTrackFactory();

		rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(const std::string& label);

		rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(const std::string& label);

	private:
		// Signaling and worker threads.
		std::unique_ptr<rtc::Thread> networkThread;
		std::unique_ptr<rtc::Thread> signalingThread;
		std::unique_ptr<rtc::Thread> workerThread;

		// PeerConnection factory.
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
};

#endif
