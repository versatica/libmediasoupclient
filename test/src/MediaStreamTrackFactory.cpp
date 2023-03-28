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

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> Factory;

/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
static rtc::Thread* NetworkThread;
static rtc::Thread* SignalingThread;
static rtc::Thread* WorkerThread;

static void createFactory()
{
	NetworkThread   = rtc::Thread::Create().release();
	SignalingThread = rtc::Thread::Create().release();
	WorkerThread    = rtc::Thread::Create().release();

	NetworkThread->SetName("network_thread", nullptr);
	SignalingThread->SetName("signaling_thread", nullptr);
	WorkerThread->SetName("worker_thread", nullptr);

	if (!NetworkThread->Start() || !SignalingThread->Start() || !WorkerThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
	if (!fakeAudioCaptureModule)
	{
		MSC_THROW_INVALID_STATE_ERROR("audio capture module creation errored");
	}

	Factory = webrtc::CreatePeerConnectionFactory(
	  NetworkThread,
	  WorkerThread,
	  SignalingThread,
	  fakeAudioCaptureModule,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  webrtc::CreateBuiltinVideoEncoderFactory(),
	  webrtc::CreateBuiltinVideoDecoderFactory(),
	  nullptr /*audio_mixer*/,
	  nullptr /*audio_processing*/);

	if (!Factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	if (!Factory)
		createFactory();

	cricket::AudioOptions options;
	options.highpass_filter = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source = Factory->CreateAudioSource(options);

	return Factory->CreateAudioTrack(label, source.get());
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	if (!Factory)
		createFactory();

	return Factory->CreateVideoTrack(label, webrtc::FakeVideoTrackSource::Create().get());
}
