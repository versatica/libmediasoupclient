/**
 * Verifies that the mediasoupclient API works end-to-end against a release-mode
 * libwebrtc build. Uses only public WebRTC API (no pc/test/ headers).
 *
 * What it tests:
 *   - PeerConnectionFactory creation with the platform default ADM
 *   - Device::Load with fake router capabilities
 *   - CreateSendTransport -> Produce (real audio track, fake signalling)
 *   - CreateRecvTransport -> Consume (fake signalling)
 *
 * No real ICE/DTLS connection is established; transports stay in "checking".
 * The value is confirming the SDP negotiation path and codec wiring work
 * correctly.
 */

#include "fakeParameters.hpp"
#include "mediasoupclient.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
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
#include "rtc_base/thread.h"
#include <catch.hpp>
#include <memory>

using json = nlohmann::json;

class SmokeSendTransportListener : public mediasoupclient::SendTransport::Listener
{
public:
	std::future<void> OnConnect(
	  mediasoupclient::Transport* /*transport*/, const json& /*dtlsParameters*/) override
	{
		std::promise<void> p;
		p.set_value();
		return p.get_future();
	}

	void OnConnectionStateChange(
	  mediasoupclient::Transport* /*transport*/,
	  const std::string& /*connectionState*/) override
	{
	}

	std::future<std::string> OnProduce(
	  mediasoupclient::SendTransport* /*transport*/,
	  const std::string& /*kind*/,
	  json /*rtpParameters*/,
	  const json& /*appData*/) override
	{
		std::promise<std::string> p;
		p.set_value(generateProducerRemoteId());
		return p.get_future();
	}

	std::future<std::string> OnProduceData(
	  mediasoupclient::SendTransport* /*transport*/,
	  const json& /*sctpStreamParameters*/,
	  const std::string& /*label*/,
	  const std::string& /*protocol*/,
	  const json& /*appData*/) override
	{
		std::promise<std::string> p;
		p.set_value(generateProducerRemoteId());
		return p.get_future();
	}
};

class SmokeRecvTransportListener : public mediasoupclient::RecvTransport::Listener
{
public:
	std::future<void> OnConnect(
	  mediasoupclient::Transport* /*transport*/, const json& /*dtlsParameters*/) override
	{
		std::promise<void> p;
		p.set_value();
		return p.get_future();
	}

	void OnConnectionStateChange(
	  mediasoupclient::Transport* /*transport*/,
	  const std::string& /*connectionState*/) override
	{
	}
};

class SmokeProducerListener : public mediasoupclient::Producer::Listener
{
public:
	void OnTransportClose(mediasoupclient::Producer* /*producer*/) override {}
};

class SmokeConsumerListener : public mediasoupclient::Consumer::Listener
{
public:
	void OnTransportClose(mediasoupclient::Consumer* /*consumer*/) override {}
};

TEST_CASE("Smoke", "[Smoke]")
{
	mediasoupclient::Initialize();

	auto networkThread   = webrtc::Thread::CreateWithSocketServer();
	auto workerThread    = webrtc::Thread::Create();
	auto signalingThread = webrtc::Thread::Create();
	networkThread->SetName("smoke_network", nullptr);
	workerThread->SetName("smoke_worker", nullptr);
	signalingThread->SetName("smoke_signaling", nullptr);
	networkThread->Start();
	workerThread->Start();
	signalingThread->Start();

	auto factory = webrtc::CreatePeerConnectionFactory(
	  networkThread.get(),
	  workerThread.get(),
	  signalingThread.get(),
	  nullptr /* ADM - platform default */,
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
	  nullptr /* audio_mixer */,
	  nullptr /* audio_processing */,
	  nullptr /* audio_frame_processor */,
	  nullptr /* field_trials */);

	REQUIRE(factory != nullptr);

	mediasoupclient::PeerConnection::Options pcOptions;
	pcOptions.factory = factory.get();

	mediasoupclient::Device device;

	SECTION("Device::Load succeeds")
	{
		REQUIRE_NOTHROW(device.Load(generateRouterRtpCapabilities(), &pcOptions));
	}

	SECTION("SendTransport and Produce audio")
	{
		REQUIRE_NOTHROW(device.Load(generateRouterRtpCapabilities(), &pcOptions));

		auto transportParams = generateTransportRemoteParameters();
		SmokeSendTransportListener sendListener;
		auto* sendTransport = device.CreateSendTransport(
		  &sendListener,
		  transportParams["id"].get<std::string>(),
		  transportParams["iceParameters"],
		  transportParams["iceCandidates"],
		  transportParams["dtlsParameters"],
		  transportParams["sctpParameters"],
		  &pcOptions);

		REQUIRE(sendTransport != nullptr);

		auto audioSource = factory->CreateAudioSource({});
		auto audioTrack  = factory->CreateAudioTrack("smoke-audio", audioSource.get());
		REQUIRE(audioTrack != nullptr);

		SmokeProducerListener producerListener;
		mediasoupclient::Producer* producer{ nullptr };
		REQUIRE_NOTHROW(
		  producer = sendTransport->Produce(&producerListener, audioTrack.get(), nullptr, nullptr, {}));
		REQUIRE(producer != nullptr);

		delete producer;
		delete sendTransport;
	}

	SECTION("RecvTransport and Consume audio")
	{
		REQUIRE_NOTHROW(device.Load(generateRouterRtpCapabilities(), &pcOptions));

		auto recvTransportParams = generateTransportRemoteParameters();
		SmokeRecvTransportListener recvListener;
		auto* recvTransport = device.CreateRecvTransport(
		  &recvListener,
		  recvTransportParams["id"].get<std::string>(),
		  recvTransportParams["iceParameters"],
		  recvTransportParams["iceCandidates"],
		  recvTransportParams["dtlsParameters"],
		  &pcOptions);

		REQUIRE(recvTransport != nullptr);

		auto consumerParams = generateConsumerRemoteParameters("audio/opus");
		SmokeConsumerListener consumerListener;
		mediasoupclient::Consumer* consumer{ nullptr };
		REQUIRE_NOTHROW(consumer = recvTransport->Consume(
		                  &consumerListener,
		                  consumerParams["id"].get<std::string>(),
		                  consumerParams["producerId"].get<std::string>(),
		                  consumerParams["kind"].get<std::string>(),
		                  &consumerParams["rtpParameters"]));
		REQUIRE(consumer != nullptr);

		delete consumer;
		delete recvTransport;
	}
}
