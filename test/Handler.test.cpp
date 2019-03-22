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

class FakeHandlerListener : public Handler::PrivateListener
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
}

TEST_CASE("SendHandler", "[Handler][SendHandler]")
{
	static rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

	static FakeHandlerListener handlerListener;

	static SendHandler sendHandler(
	  &handlerListener,
	  TransportRemoteParameters["iceParameters"],
	  TransportRemoteParameters["iceCandidates"],
	  TransportRemoteParameters["dtlsParameters"],
	  &PeerConnectionOptions,
	  RtpParametersByKind,
	  RtpParametersByKind);

	static std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, &PeerConnectionOptions));

	static std::string localId;

	SECTION("'sendHandler.Send()' fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(nullptr, nullptr, nullptr), Exception);
	}

	SECTION("'sendHandler.Send()' succeeds if a track is provided")
	{
		track = createAudioTrack("test-track-id");

		std::pair<std::string, nlohmann::json> result;

		REQUIRE_NOTHROW(result = sendHandler.Send(track, nullptr, nullptr));

		localId            = result.first;
		json rtpParameters = result.second;

		REQUIRE(rtpParameters["codecs"].size() == 1);
		REQUIRE(rtpParameters["headerExtensions"].size() == 3);
	}

	SECTION("'sendHandler.Send()' succeeds if track is already handled")
	{
		REQUIRE_NOTHROW(sendHandler.Send(track, nullptr, nullptr));
	}

	SECTION("'sendHandler.ReplaceTrack()' fails if an invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.ReplaceTrack("", nullptr), Exception);
	}

	SECTION("'sendHandler.ReplaceTrack()' succeeds if a new track is provided")
	{
		auto newTrack = createAudioTrack("test-new-track-id");

		REQUIRE_NOTHROW(sendHandler.ReplaceTrack(localId, newTrack));

		track = newTrack;
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' fails if invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.SetMaxSpatialLayer("", 1), Exception);
	}

	SECTION("'sendHandler.SetMaxSpatialLayer()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.SetMaxSpatialLayer(localId, 1));
	}

	SECTION("'sendHandler.GetSenderStats()' fails if invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.GetSenderStats(""), Exception);
	}

	SECTION("'sendHandler.GetSenderStats()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.GetSenderStats(localId));
	}

	SECTION("'sendHandler.StopSending()' fails if an invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(""), Exception);
	}

	SECTION("'sendHandler.StopSending()' succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.StopSending(localId));
	}

	SECTION("'sendHandler.RestartIce()' succeeds")
	{
		auto iceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendHandler.RestartIce(iceParameters));
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

	static std::string localId;

	static FakeHandlerListener handlerListener;

	static RecvHandler recvHandler(
	  &handlerListener,
	  TransportRemoteParameters["iceParameters"],
	  TransportRemoteParameters["iceCandidates"],
	  TransportRemoteParameters["dtlsParameters"],
	  &PeerConnectionOptions);

	SECTION("'recvHander.Receive()' succeeds if correct rtpParameters are provided")
	{
		std::pair<std::string, webrtc::MediaStreamTrackInterface*> result;

		REQUIRE_NOTHROW(result = recvHandler.Receive("test", "audio", &rtpParameters));

		localId = result.first;
	}

	SECTION("'recvHandler.GetReceiverStats()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.GetReceiverStats("unknown"), Exception);
	}

	SECTION("'recvHandler.GetReceiverStats()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.GetReceiverStats(localId));
	}

	SECTION("'recvHandler.StopReceiving()' fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.StopReceiving("unknown"), Exception);
	}

	SECTION("'recvHandler.StopReceiving()' succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.StopReceiving(localId));
	}

	SECTION("'recvHandler.RestartIce()' succeeds")
	{
		auto iceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvHandler.RestartIce(iceParameters));
	}

	SECTION("'recvHandler.UpdateIceServers()' succeeds")
	{
		REQUIRE_NOTHROW(recvHandler.UpdateIceServers(json::array()));
	}
}
