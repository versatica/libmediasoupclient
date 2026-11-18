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

	protected:
		webrtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
		{
			return nullptr;
		}
	};
} // namespace

void MediaStreamTrackFactory::Create()
{
	mediasoupclient::Initialize();

	if (factory)
	{
		return;
	}

	networkThread   = webrtc::Thread::CreateWithSocketServer();
	workerThread    = webrtc::Thread::Create();
	signalingThread = webrtc::Thread::Create();
	networkThread->SetName("network_thread", nullptr);
	workerThread->SetName("worker_thread", nullptr);
	signalingThread->SetName("signaling_thread", nullptr);

	if (!networkThread->Start() || !workerThread->Start() || !signalingThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	// nullptr ADM -> platform default (CoreAudio/ALSA/WASAPI). No pc/test/ headers needed.
	factory = webrtc::CreatePeerConnectionFactory(
	  networkThread.get(),
	  workerThread.get(),
	  signalingThread.get(),
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

	if (!factory)
	{
		MSC_THROW_INVALID_STATE_ERROR("peer connection factory creation errored");
	}

	peerConnectionOptions.factory = factory.get();
}

void MediaStreamTrackFactory::Destroy()
{
	factory     = nullptr;
	audioSource = nullptr;
}

webrtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& id)
{
	auto& f = MediaStreamTrackFactory::GetInstance();

	if (!f.audioSource)
	{
		f.audioSource = f.factory->CreateAudioSource({});
	}

	return f.factory->CreateAudioTrack(id, f.audioSource.get());
}

webrtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& id)
{
	const auto& f     = MediaStreamTrackFactory::GetInstance();
	const auto source = webrtc::make_ref_counted<NullVideoTrackSource>();

	return f.factory->CreateVideoTrack(source, id);
}
