#define MSC_CLASS "MediaStreamTrackFactory"

#include "MediaStreamTrackFactory.hpp"
#include "MediaSoupClientErrors.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_video_track_source.h"

using namespace mediasoupclient;

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;

/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
static rtc::Thread* networkThread;
static rtc::Thread* signalingThread;
static rtc::Thread* workerThread;

static void createFactory()
{
	networkThread   = rtc::Thread::Create().release();
	signalingThread = rtc::Thread::Create().release();
	workerThread    = rtc::Thread::Create().release();

	networkThread->SetName("network_thread", nullptr);
	signalingThread->SetName("signaling_thread", nullptr);
	workerThread->SetName("worker_thread", nullptr);

	if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
	if (!fakeAudioCaptureModule)
	{
		MSC_THROW_INVALID_STATE_ERROR("audio capture module creation errored");
	}

	factory = webrtc::CreatePeerConnectionFactory(
	  networkThread,
	  workerThread,
	  signalingThread,
	  fakeAudioCaptureModule,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  webrtc::CreateBuiltinVideoEncoderFactory(),
	  webrtc::CreateBuiltinVideoDecoderFactory(),
	  nullptr /*audio_mixer*/,
	  nullptr /*audio_processing*/);

	if (!factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	if (!factory)
		createFactory();

	cricket::AudioOptions options;
	options.highpass_filter = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source = factory->CreateAudioSource(options);

	return factory->CreateAudioTrack(label, source);
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	if (!factory)
		createFactory();

	return factory->CreateVideoTrack(label, webrtc::FakeVideoTrackSource::Create());
}
