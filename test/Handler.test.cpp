#include "Exception.hpp"
#include "Handler.hpp"
#include "catch.hpp"
#include "data/parameters.hpp"
#include "peerConnectionUtils.hpp"
#include <memory>

using namespace mediasoupclient;

static const json TransportRemoteParameters = generateTransportRemoteParameters();
static const json RtpParametersByKind       = generateRtpParametersByKind();

static PeerConnection::Options PeerConnectionOptions;

class FakeHandlerListener : public Handler::Listener
{
public:
	void OnConnect(json& /*transportLocalParameters*/) override{};

	void OnConnectionStateChange(
	  webrtc::PeerConnectionInterface::IceConnectionState /*connectionState*/) override{};
};

TEST_CASE("Handler", "[Handler]")
{
	SECTION("'Handler::GetNativeRtpCapabilities()' succeeds")
	{
		json rtpCapabilities;

		REQUIRE_NOTHROW(rtpCapabilities = Handler::GetNativeRtpCapabilities());

		REQUIRE(rtpCapabilities["codecs"].is_array());
		REQUIRE(rtpCapabilities["fecMechanisms"].is_array());
		REQUIRE(rtpCapabilities["headerExtensions"].is_array());
	}

	SECTION("'Handler::GetName()' succeeds")
	{
		REQUIRE_NOTHROW(Handler::GetName());
	}
}

TEST_CASE("SendHandler", "[Handler][SendHandler]")
{
	static rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

	static FakeHandlerListener handlerListener;

	static SendHandler sendHandler(
	  &handlerListener, TransportRemoteParameters, &PeerConnectionOptions, RtpParametersByKind);

	static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, &PeerConnectionOptions));

	static const std::vector<webrtc::RtpEncodingParameters> Encodings;

	SECTION("'sendHandler.Send()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(nullptr, Encodings), Exception);
	}

	SECTION("'sendHandler.Send()' succeeds if a track is provided")
	{
		track = createAudioTrack("test-track-id");

		json rtpParameters;

		REQUIRE_NOTHROW(rtpParameters = sendHandler.Send(track, Encodings));
		REQUIRE(rtpParameters["codecs"].size() == 1);
		REQUIRE(rtpParameters["headerExtensions"].size() == 3);
	}

	SECTION("'sendHandler.Send()' fails if track is already handled")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(track, Encodings), Exception);
	}

	SECTION("'sendHandler.ReplaceTrack()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.ReplaceTrack(nullptr, nullptr), Exception);
	}

	SECTION("'sendHandler.ReplaceTrack()' succeeds if a new track is provided")
	{
		auto newTrack = createAudioTrack("test-new-track-id");

		REQUIRE_NOTHROW(sendHandler.ReplaceTrack(track, newTrack));

		track = newTrack;
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.SetMaxSpatialLayer(nullptr, 1), Exception);
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.SetMaxSpatialLayer(track, 1));
	}

	SECTION("'sendHandler.GetSenderStats()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.GetSenderStats(nullptr), Exception);
	}

	SECTION("'sendHandler.GetSenderStats()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.GetSenderStats(track));
	}

	SECTION("'sendHandler.StopSending()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(nullptr), Exception);
	}

	SECTION("'sendHandler.StopSending()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.StopSending(track));
	}

	SECTION("'sendHandler.StopSending()' fails if track is not being sent")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(track), Exception);
	}

	SECTION("'sendHandler.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendHandler.RestartIce(remoteIceParameters));
	}

	SECTION("'sendHandler.UpdateIceServers()' succeeds")
	{
		REQUIRE_NOTHROW(sendHandler.UpdateIceServers(json::array()));
	}
}

TEST_CASE("RecvHandler", "[Handler][RecvHandler]")
{
	auto consumerRemoteParameters = generateConsumerRemoteParameters("audio/opus");
	auto producerId               = consumerRemoteParameters["producerId"].get<std::string>();
	auto id                       = consumerRemoteParameters["id"].get<std::string>();
	auto kind                     = consumerRemoteParameters["kind"].get<std::string>();
	auto rtpParameters            = consumerRemoteParameters["rtpParameters"];

	static FakeHandlerListener handlerListener;

	static RecvHandler recvHandler(&handlerListener, TransportRemoteParameters, &PeerConnectionOptions);

	SECTION("'recvHander.Receive()' succeeds if correct rtpParameters are provided")
	{
		REQUIRE_NOTHROW(recvHandler.Receive("test", "audio", rtpParameters));
	}

	SECTION("'recvHandler.Receive()' fails if rtpParameters are already handled")
	{
		REQUIRE_THROWS_AS(recvHandler.Receive("test", "audio", rtpParameters), Exception);
	}

	SECTION("'recvHandler.GetReceiverStats()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.GetReceiverStats("unknown"), Exception);
	}

	SECTION("'recvHandler.GetReceiverStats()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.GetReceiverStats("test"));
	}

	SECTION("'recvHandler.StopReceiving()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.StopReceiving("unknown"), Exception);
	}

	SECTION("'recvHandler.StopReceiving()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.StopReceiving("test"));
	}

	SECTION("'recvHandler.RestartIce()' succeeds")
	{
		auto remoteIceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvHandler.RestartIce(remoteIceParameters));
	}

	SECTION("'recvHandler.UpdateIceServers()' succeeds")
	{
		REQUIRE_NOTHROW(recvHandler.UpdateIceServers(json::array()));
	}
}
