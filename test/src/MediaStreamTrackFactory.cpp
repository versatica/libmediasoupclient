#define MSC_CLASS "MediaStreamTrackFactory"

#include "MediaStreamTrackFactory.hpp"
#include "MediaSoupClientErrors.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/make_ref_counted.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "mediasoupclient.hpp"
#include "pc/video_track_source.h"

using namespace mediasoupclient;

// Minimal null video source using only public WebRTC API.
// No pc/test/ dependency; safe to link against a release-mode libwebrtc.
namespace
{
	class NullVideoTrackSource : public webrtc::VideoTrackSource
	{
	public:
		NullVideoTrackSource() : webrtc::VideoTrackSource(/*remote=*/false)
		{
		}

		webrtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
		{
			return nullptr;
		}

	protected:
		// VideoTrackSource::AddOrUpdateSink/RemoveSink forward to source(), which is
		// null here. Override them as no-ops to prevent the null dereference.
		void AddOrUpdateSink(
		  webrtc::VideoSinkInterface<webrtc::VideoFrame>* /*sink*/,
		  const webrtc::VideoSinkWants& /*wants*/) override
		{
		}

		void RemoveSink(webrtc::VideoSinkInterface<webrtc::VideoFrame>* /*sink*/) override
		{
		}
	};
} // namespace

void MediaStreamTrackFactory::Create()
{
	mediasoupclient::Initialize();

	if (Factory)
	{
		return;
	}

	NetworkThread   = webrtc::Thread::CreateWithSocketServer();
	WorkerThread    = webrtc::Thread::Create();
	SignalingThread = webrtc::Thread::Create();
	NetworkThread->SetName("network_thread", nullptr);
	WorkerThread->SetName("worker_thread", nullptr);
	SignalingThread->SetName("signaling_thread", nullptr);

	if (!NetworkThread->Start() || !WorkerThread->Start() || !SignalingThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	// nullptr ADM → platform default (CoreAudio/ALSA/WASAPI). No pc/test/ headers needed.
	Factory = webrtc::CreatePeerConnectionFactory(
	  NetworkThread.get(),
	  WorkerThread.get(),
	  SignalingThread.get(),
	  nullptr /* ADM */,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  std::make_unique<webrtc::VideoEncoderFactoryTemplate<
	    webrtc::LibvpxVp8EncoderTemplateAdapter,
	    webrtc::LibvpxVp9EncoderTemplateAdapter,
	    webrtc::OpenH264EncoderTemplateAdapter,
	    webrtc::LibaomAv1EncoderTemplateAdapter>>(),
	  std::make_unique<webrtc::VideoDecoderFactoryTemplate<
	    webrtc::LibvpxVp8DecoderTemplateAdapter,
	    webrtc::LibvpxVp9DecoderTemplateAdapter,
	    webrtc::OpenH264DecoderTemplateAdapter,
	    webrtc::Dav1dDecoderTemplateAdapter>>(),
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr);

	if (!Factory)
	{
		MSC_THROW_INVALID_STATE_ERROR("peer connection factory creation errored");
	}

	PeerConnectionOptions.factory = Factory.get();
}

void MediaStreamTrackFactory::Destroy()
{
	Factory     = nullptr;
	AudioSource = nullptr;
}

webrtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& id)
{
	auto& f = MediaStreamTrackFactory::getInstance();

	if (!f.AudioSource)
	{
		f.AudioSource = f.Factory->CreateAudioSource({});
	}

	return f.Factory->CreateAudioTrack(id, f.AudioSource.get());
}

webrtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& id)
{
	auto& f     = MediaStreamTrackFactory::getInstance();
	auto source = webrtc::make_ref_counted<NullVideoTrackSource>();

	return f.Factory->CreateVideoTrack(source, id);
}
