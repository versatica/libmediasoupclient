#include "Device.hpp"
#include "FakeTransportListener.hpp"
#include "MediaSoupClientErrors.hpp"
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
#include "fakeParameters.hpp"
#include "ortc.hpp"
#include "rtc_base/thread.h"
#include <catch.hpp>

TEST_CASE("Device", "[Device]")
{
	static const json TransportRemoteParameters = generateTransportRemoteParameters();

	static FakeSendTransportListener sendTransportListener;
	static FakeRecvTransportListener recvTransportListener;

	static std::unique_ptr<mediasoupclient::Device> device(new mediasoupclient::Device());

	static json routerRtpCapabilities;

	SECTION("device.IsLoaded() is false if not loaded")
	{
		REQUIRE(!device->IsLoaded());
	}

	SECTION("device.GetRtpCapabilities() throws if not loaded")
	{
		REQUIRE_THROWS_AS(device->GetRtpCapabilities(), MediaSoupClientInvalidStateError);
	}

	SECTION("device.CanProduce() with audio/video throws if not loaded")
	{
		REQUIRE_THROWS_AS(device->CanProduce("audio"), MediaSoupClientInvalidStateError);
		REQUIRE_THROWS_AS(device->CanProduce("video"), MediaSoupClientInvalidStateError);
	}

	SECTION("device.CreateSendTransport() fails if not loaded")
	{
		REQUIRE_THROWS_AS(
		  device->CreateSendTransport(
		    &sendTransportListener,
		    TransportRemoteParameters["id"],
		    TransportRemoteParameters["iceParameters"],
		    TransportRemoteParameters["iceCandidates"],
		    TransportRemoteParameters["dtlsParameters"]),
		  MediaSoupClientInvalidStateError);
	}

	SECTION("device.CreateRecvTransport() fails if not loaded")
	{
		REQUIRE_THROWS_AS(
		  device->CreateRecvTransport(
		    &recvTransportListener,
		    TransportRemoteParameters["id"],
		    TransportRemoteParameters["iceParameters"],
		    TransportRemoteParameters["iceCandidates"],
		    TransportRemoteParameters["dtlsParameters"]),
		  MediaSoupClientInvalidStateError);
	}

	SECTION("device.Load() succeeds")
	{
		routerRtpCapabilities = generateRouterRtpCapabilities();

		REQUIRE_NOTHROW(device->Load(routerRtpCapabilities));
		REQUIRE(device->IsLoaded());
	}

	SECTION("device.Load() fails if already loaded")
	{
		REQUIRE_THROWS_AS(device->Load(routerRtpCapabilities), MediaSoupClientInvalidStateError);
	}

	SECTION("device.GetRtpCapabilities() succeeds")
	{
		REQUIRE(device->GetRtpCapabilities().is_object());
	}

	SECTION("device.CanProduce() with 'audio'/'video' kind returns true")
	{
		REQUIRE(device->CanProduce("audio"));
		REQUIRE(device->CanProduce("video"));
	}

	SECTION("device.CanProduce() with invalid kind throws exception")
	{
		REQUIRE_THROWS_AS(device->CanProduce("chicken"), MediaSoupClientError);
	}

	SECTION("device.CreateSendTransport() succeeds")
	{
		REQUIRE_NOTHROW(device->CreateSendTransport(
		  &sendTransportListener,
		  TransportRemoteParameters["id"],
		  TransportRemoteParameters["iceParameters"],
		  TransportRemoteParameters["iceCandidates"],
		  TransportRemoteParameters["dtlsParameters"]));
	}

	SECTION("device.CreateRecvTransport() succeeds")
	{
		REQUIRE_NOTHROW(device->CreateRecvTransport(
		  &recvTransportListener,
		  TransportRemoteParameters["id"],
		  TransportRemoteParameters["iceParameters"],
		  TransportRemoteParameters["iceCandidates"],
		  TransportRemoteParameters["dtlsParameters"]));
	}
}

/**
 * Regression test: device->Load() must not deadlock when called from the
 * signaling thread of a custom PeerConnectionFactory.
 *
 * Root cause: PeerConnection methods used future.get() which blocked the
 * calling thread. WebRTC observer callbacks are delivered on the signaling
 * thread, so calling from that thread deadlocked.
 *
 * Fix: waitForSignalingCallback() pumps the current WebRTC thread's message
 * queue while waiting, allowing observer callbacks to fire.
 *
 * https://github.com/versatica/libmediasoupclient/issues/187
 */
TEST_CASE("Device::Load from signaling thread does not deadlock", "[Device]")
{
	auto networkThread   = webrtc::Thread::CreateWithSocketServer();
	auto workerThread    = webrtc::Thread::Create();
	auto signalingThread = webrtc::Thread::Create();
	networkThread->SetName("deadlock_test_network", nullptr);
	workerThread->SetName("deadlock_test_worker", nullptr);
	signalingThread->SetName("deadlock_test_signaling", nullptr);
	networkThread->Start();
	workerThread->Start();
	signalingThread->Start();

	auto factory = webrtc::CreatePeerConnectionFactory(
	  networkThread.get(),
	  workerThread.get(),
	  signalingThread.get(),
	  nullptr,
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

	REQUIRE(factory != nullptr);

	mediasoupclient::PeerConnection::Options pcOptions;
	pcOptions.factory = factory.get();

	// Call device->Load() FROM the signaling thread - the exact scenario
	// from the bug report.
	bool loaded{ false };
	signalingThread->BlockingCall(
	  [&]()
	  {
		  mediasoupclient::Device device;
		  REQUIRE_NOTHROW(device.Load(generateRouterRtpCapabilities(), &pcOptions));
		  loaded = device.IsLoaded();
	  });

	REQUIRE(loaded);
}
