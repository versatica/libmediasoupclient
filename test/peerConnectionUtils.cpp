#include "peerConnectionUtils.hpp"
#include "media/base/fakevideocapturer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory{ nullptr };

static std::unique_ptr<cricket::FakeVideoCapturer> videoCapturer{ nullptr };

static rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource{ nullptr };
static rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource{ nullptr };

static rtc::Thread* signalingThread{ nullptr };
static rtc::Thread* workerThread{ nullptr };

// PeerConnection factory creation.
static void createPeerConnectionFactory()
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;

	signalingThread = new rtc::Thread();
	workerThread    = new rtc::Thread();

	signalingThread->SetName("signaling_thread", nullptr);
	workerThread->SetName("worker_thread", nullptr);

	if (!signalingThread->Start() || !workerThread->Start())
	{
		throw std::runtime_error("Thread start errored");
	}

	peerConnectionFactory = webrtc::CreatePeerConnectionFactory(
	  workerThread,
	  workerThread,
	  signalingThread,
	  nullptr /*default_adm*/,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  webrtc::CreateBuiltinVideoEncoderFactory(),
	  webrtc::CreateBuiltinVideoDecoderFactory(),
	  nullptr /*audio_mixer*/,
	  nullptr /*audio_processing*/);
}

// Audio source creation.
static void createAudioSource()
{
	cricket::AudioOptions options;

	if (peerConnectionFactory == nullptr)
		createPeerConnectionFactory();

	audioSource = peerConnectionFactory->CreateAudioSource(options);
}

static void createVideoCapturer()
{
	videoCapturer.reset(new cricket::FakeVideoCapturer());
}

// Video source creation.
static void createVideoSource(const webrtc::MediaConstraintsInterface* constraints)
{
	if (peerConnectionFactory == nullptr)
		createPeerConnectionFactory();

	if (videoCapturer == nullptr)
		createVideoCapturer();

	videoSource = peerConnectionFactory->CreateVideoSource(std::move(videoCapturer), constraints);
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	if (audioSource == nullptr)
		createAudioSource();

	return peerConnectionFactory->CreateAudioTrack(label, audioSource);
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	if (videoSource == nullptr)
		createVideoSource(nullptr);

	return peerConnectionFactory->CreateVideoTrack(label, videoSource);
}
