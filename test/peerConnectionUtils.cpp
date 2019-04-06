#include "peerConnectionUtils.hpp"
#include "VcmCapturer.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/video_track_source.h"
#include "modules/video_capture/video_capture_factory.h"
#include "modules/video_capture/video_capture.h"
#include <iostream>

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory{ nullptr };

static rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource{ nullptr };

class CapturerTrackSource;
static rtc::scoped_refptr<CapturerTrackSource> videoDevice{ nullptr };

static rtc::Thread* signalingThread{ nullptr };
static rtc::Thread* workerThread{ nullptr };

class CapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<CapturerTrackSource> Create() {
    const size_t kWidth = 640;
    const size_t kHeight = 480;
    const size_t kFps = 30;
    const size_t kDeviceIndex = 0;
    std::unique_ptr<VcmCapturer> capturer = absl::WrapUnique(
        VcmCapturer::Create(kWidth, kHeight, kFps, kDeviceIndex));
    if (!capturer) {
      return nullptr;
    }
    return new rtc::RefCountedObject<CapturerTrackSource>(std::move(capturer));
  }

 protected:
  explicit CapturerTrackSource(
      std::unique_ptr<VcmCapturer> capturer)
      : VideoTrackSource(/*remote=*/false), capturer(std::move(capturer)) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return capturer.get();
  }
  std::unique_ptr<VcmCapturer> capturer;
};

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
	if (videoDevice == nullptr)
		videoDevice = CapturerTrackSource::Create();

	return peerConnectionFactory->CreateVideoTrack(label, videoDevice);
}
