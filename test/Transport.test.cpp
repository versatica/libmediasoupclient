#include "Exception.hpp"
#include "FakeTransportListener.hpp"
#include "Transport.hpp"
#include "catch.hpp"
#include "data/parameters.hpp"
#include "media/base/fakevideocapturer.h"
#include "ortc.hpp"
#include <iostream>
#include <map>
#include <memory>

TEST_CASE("SendTransport", "[Transport][SendTransport]")
{
	static const json TransportRemoteParameters = generateTransportRemoteParameters();
	static const json IceServers                = json::array();
	static const json ProprietaryConstraints    = json::array();
	static const json RtpParametersByKind       = generateRtpParametersByKind();
	static const std::string IceTransportPolicy("all");

	static rtc::scoped_refptr<webrtc::AudioSourceInterface> source;
	static rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

	static json remoteCaps = generateRouterRtpCapabilities();
	static json localCaps  = generateRouterRtpCapabilities();

	static json extendedRtpCapabilities = ortc::getExtendedCapabilities(localCaps, remoteCaps);

	/* clang-format off */
	static std::map<std::string, bool> canProduceByKind =
	{
		{ "audio", true },
		{ "video", true }
	};
	/* clang-format on */

	static FakeSendTransportListener transportListener;
	static FakeProducerPublicListener producerPublicListener;

	static SendTransport sendTransport(
	  &transportListener,
	  TransportRemoteParameters,
	  IceServers,
	  IceTransportPolicy,
	  ProprietaryConstraints,
	  extendedRtpCapabilities,
	  canProduceByKind);

	static rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource;
	static rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;
	static Producer* producer{ nullptr };
	static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

	SECTION("'sendTransport.GetId()' returns the correct value")
	{
		REQUIRE(sendTransport.GetId() == TransportRemoteParameters["id"].get<std::string>());
	}

	SECTION("'sendTransport.GetConnectionState()' is 'new'")
	{
		REQUIRE(sendTransport.GetConnectionState() == "new");
	}

	SECTION("'sendTransport.GetAppData()' is nullptr")
	{
		REQUIRE(sendTransport.GetAppData().empty());
	}

	SECTION("'sendTransport.GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(sendTransport.GetStats().get());
	}

	SECTION("'sendTransport.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendTransport.RestartIce(remoteIceParameters).get());
	}

	SECTION("'sendTransport.UpdateIceServers()' succeeds")
	{
		auto iceServers = json::array();

		REQUIRE_NOTHROW(sendTransport.UpdateIceServers(iceServers).get());
	}

	// Issue (See Notes.txt)
	SECTION("'sendTransport.Produce()' audio succeeds")
	{
		static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

		auto source = pc->CreateAudioSource();
		auto track  = pc->CreateAudioTrack("audio-track-id", source);

		auto future = sendTransport.Produce(
		  &producerPublicListener, track, false /* simulcast */
		);

		REQUIRE_NOTHROW(future.get());
	}

	SECTION("'sendTransport.Produce()' video succeeds")
	{
		std::unique_ptr<cricket::FakeVideoCapturer> capturer(new cricket::FakeVideoCapturer());

		videoSource = pc->CreateVideoSource(std::move(capturer), nullptr);

		videoTrack = pc->CreateVideoTrack("video-track-id", videoSource);

		auto future = sendTransport.Produce(
		  &producerPublicListener,
		  videoTrack,
		  true /* simulcast */,
		  "high" /* maxSpatialLayer */,
		  nullptr /* appData */
		);

		REQUIRE_NOTHROW(producer = future.get());
		REQUIRE(
		  transportListener.onConnectTimesCalled == ++transportListener.onConnectExpectedTimesCalled);
		REQUIRE(
		  transportListener.onProduceExpectedTimesCalled ==
		  ++transportListener.onProduceExpectedTimesCalled);

		REQUIRE(producer->GetId() == "producer-id");
		REQUIRE(!producer->IsClosed());
		REQUIRE(!producer->IsPaused());
		REQUIRE(producer->GetTrack() == videoTrack);
		REQUIRE(producer->GetAppData().empty());
	}

	SECTION("'producer->Pause()' succeeds")
	{
		producer->Pause();

		REQUIRE(producer->IsPaused());
	}

	SECTION("'producer->Resume()' succeeds")
	{
		producer->Resume();

		REQUIRE(!producer->IsPaused());
	}

	SECTION("'producer->ReplaceTrack()' fails if null track is provided")
	{
		REQUIRE_THROWS_AS(producer->ReplaceTrack(nullptr).get(), Exception);
	}

	SECTION("'producer->ReplaceTrack()' succeeds")
	{
		auto newTrack = pc->CreateVideoTrack("video-track-id-2", videoSource);

		REQUIRE_NOTHROW(producer->ReplaceTrack(newTrack).get());
		REQUIRE(producer->GetTrack() == newTrack);
	}

	SECTION("'producer->SetMaxSpatialLayer()' fails if invalid spatial layer is proivded")
	{
		REQUIRE_THROWS_AS(producer->SetMaxSpatialLayer("invalid").get(), Exception);
	}

	SECTION("'producer->SetMaxSpatialLayer()' succeeds if valid spatial layer is proivded")
	{
		REQUIRE_NOTHROW(producer->SetMaxSpatialLayer("low").get());
		REQUIRE(producer->GetMaxSpatialLayer() == "low");
	}

	SECTION("'producer->GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(producer->GetStats().get());
	}

	SECTION("'producer->Close()' succeeds")
	{
		producer->Close();

		REQUIRE(producer->IsClosed());
	}

	SECTION("'producer->TransportClosed()' succeeds")
	{
		producer->TransportClosed();

		REQUIRE(producer->IsClosed());
	}
}

TEST_CASE("RecvTransport", "[Transport][RecvTransport]")
{
	static const json TransportRemoteParameters = generateTransportRemoteParameters();
	static const json IceServers                = json::array();
	static const json ProprietaryConstraints    = json::array();
	static const json RtpParametersByKind       = generateRtpParametersByKind();
	static const std::string IceTransportPolicy("all");

	static json remoteCaps = generateRouterRtpCapabilities();
	static json localCaps  = generateRouterRtpCapabilities();

	static json extendedRtpCapabilities = ortc::getExtendedCapabilities(localCaps, remoteCaps);

	static FakeRecvTransportListener transportListener;
	static FakeConsumerPublicListener consumerPublicListener;

	static RecvTransport recvTransport(
	  &transportListener,
	  TransportRemoteParameters,
	  IceServers,
	  IceTransportPolicy,
	  ProprietaryConstraints,
	  extendedRtpCapabilities);

	static Consumer* consumer{ nullptr };
	static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

	SECTION("'recvTransport.GetId()' returns the correct value")
	{
		REQUIRE(recvTransport.GetId() == TransportRemoteParameters["id"].get<std::string>());
	}

	SECTION("'recvTransport.GetConnectionState()' is 'new'")
	{
		REQUIRE(recvTransport.GetConnectionState() == "new");
	}

	SECTION("'recvTransport.GetAppData()' is nullptr")
	{
		REQUIRE(recvTransport.GetAppData().empty());
	}

	SECTION("'recvTransport.GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(recvTransport.GetStats().get());
	}

	SECTION("'recvTransport.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvTransport.RestartIce(remoteIceParameters).get());
	}

	SECTION("'recvTransport.UpdateIceServers()' succeeds")
	{
		auto iceServers = json::array();

		REQUIRE_NOTHROW(recvTransport.UpdateIceServers(iceServers).get());
	}

	SECTION("'recvTransport.Consume()' audio succeeds")
	{
		static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

		auto audioConsumerRemoteParameters = generateConsumerRemoteParameters("audio/opus");
		auto videoConsumerRemoteParameters = generateConsumerRemoteParameters("video/VP8");

		auto future =
		  recvTransport.Consume(&consumerPublicListener, audioConsumerRemoteParameters, nullptr);

		REQUIRE_NOTHROW(consumer = future.get());
		REQUIRE(
		  transportListener.onConnectTimesCalled == ++transportListener.onConnectExpectedTimesCalled);
		REQUIRE(
		  transportListener.onStartConsumerTimesCalled ==
		  ++transportListener.onStartConsumerExpectedTimesCalled);
		REQUIRE(consumer->GetId() == audioConsumerRemoteParameters["id"].get<std::string>());
		REQUIRE(
		  consumer->GetProducerId() == audioConsumerRemoteParameters["producerId"].get<std::string>());
		REQUIRE(!consumer->IsClosed());
		REQUIRE(consumer->GetKind() == "audio");
		REQUIRE(consumer->GetTrack() != nullptr);
		REQUIRE(consumer->GetRtpParameters().is_object());
		REQUIRE(consumer->GetRtpParameters().find("mid") == consumer->GetRtpParameters().end());
		REQUIRE(consumer->GetRtpParameters()["codecs"].size() == 1);

		auto codecs = consumer->GetRtpParameters()["codecs"];

		REQUIRE(codecs[0] == R"(
			{
				"name"         : "opus",
				"mimeType"     : "audio/opus",
				"clockRate"    : 48000,
				"payloadType"  : 100,
				"channels"     : 2,
				"rtcpFeedback" : [],
				"parameters"   :
				{
					"useinbandfec" : "1"
				}
			})"_json);
	}

	SECTION("'consumer->Pause()' succeeds")
	{
		consumer->Pause();

		REQUIRE(consumer->IsPaused());
	}

	SECTION("'consumer->Resume()' succeeds")
	{
		consumer->Resume();

		REQUIRE(!consumer->IsPaused());
	}

	SECTION("'consumer->GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(consumer->GetStats().get());
	}

	SECTION("'consumer->Close()' succeeds")
	{
		consumer->Close();

		REQUIRE(consumer->IsClosed());
	}

	SECTION("'consumer->TransportClosed()' succeeds")
	{
		consumer->TransportClosed();

		REQUIRE(consumer->IsClosed());
	}
}
