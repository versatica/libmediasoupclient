#include "peerConnectionUtils.hpp"
#include "VcmCapturer.hpp"
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <pc/video_track_source.h>
#include <iostream>
#include <modules/audio_device/include/fake_audio_device.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>

class CapturerTrackSource;

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory{ nullptr };
static rtc::scoped_refptr<webrtc::AudioSourceInterface> AudioSource{ nullptr };
static rtc::scoped_refptr<CapturerTrackSource> VideoDevice{ nullptr };
static rtc::Thread* SignalingThread{ nullptr };
static rtc::Thread* WorkerThread{ nullptr };

class CapturerTrackSource : public webrtc::VideoTrackSource
{
public:
	static rtc::scoped_refptr<CapturerTrackSource> Create()
	{
		const size_t kWidth       = 640;
		const size_t kHeight      = 480;
		const size_t kFps         = 30;
		const size_t kDeviceIndex = 0;

		std::unique_ptr<VcmCapturer> capturer =
		  absl::WrapUnique(VcmCapturer::Create(kWidth, kHeight, kFps, kDeviceIndex));

		if (!capturer)
			return nullptr;

		return new rtc::RefCountedObject<CapturerTrackSource>(std::move(capturer));
	}

protected:
	explicit CapturerTrackSource(std::unique_ptr<VcmCapturer> capturer)
	  : VideoTrackSource(/*remote=*/false), capturer(std::move(capturer))
	{
	}

private:
	rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
	{
		return capturer.get();
	}

	std::unique_ptr<VcmCapturer> capturer;
};

// Audio source creation.
static void createAudioSource()
{
	cricket::AudioOptions options;

	if (PeerConnectionFactory == nullptr)
		PeerConnectionFactory = createPeerConnectionFactory();

	AudioSource = PeerConnectionFactory->CreateAudioSource(options);
}

// PeerConnection factory creation.
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createPeerConnectionFactory()
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;

	SignalingThread = new rtc::Thread();
	WorkerThread    = new rtc::Thread();

	SignalingThread->SetName("signaling_thread", nullptr);
	WorkerThread->SetName("worker_thread", nullptr);

	if (!SignalingThread->Start() || !WorkerThread->Start())
		throw std::runtime_error("Thread start errored");

	rtc::scoped_refptr<webrtc::AudioDeviceModule> audioDeviceModule(new webrtc::FakeAudioDeviceModule());

	PeerConnectionFactory = webrtc::CreatePeerConnectionFactory(
	  WorkerThread,
	  WorkerThread,
	  SignalingThread,
	  audioDeviceModule,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  webrtc::CreateBuiltinVideoEncoderFactory(),
	  webrtc::CreateBuiltinVideoDecoderFactory(),
	  nullptr /*audio_mixer_module*/,
	  nullptr /*audio_processing_module*/);

	return PeerConnectionFactory;
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	if (AudioSource == nullptr)
		createAudioSource();

	return PeerConnectionFactory->CreateAudioTrack(label, AudioSource);
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	if (PeerConnectionFactory == nullptr)
		PeerConnectionFactory = createPeerConnectionFactory();

	if (VideoDevice == nullptr)
		VideoDevice = CapturerTrackSource::Create();

	return PeerConnectionFactory->CreateVideoTrack(label, VideoDevice);
}
