#include "Handler.hpp"
#include "MediaSoupClientErrors.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "fakeParameters.hpp"
#include <catch.hpp>
#include <iostream>
#include <memory>

static const json TransportRemoteParameters = generateTransportRemoteParameters();
static const json RtpParametersByKind       = generateRtpParametersByKind();

static mediasoupclient::PeerConnection::Options PeerConnectionOptions;

class FakeHandlerListener : public mediasoupclient::Handler::PrivateListener
{
public:
	void OnConnect(json& /*transportLocalParameters*/) override{};

	void OnConnectionStateChange(
	  webrtc::PeerConnectionInterface::IceConnectionState /*connectionState*/) override{};
};

TEST_CASE("Handler", "[Handler]")
{
	SECTION("Handler::GetNativeRtpCapabilities() succeeds")
	{
		json rtpCapabilities;

		REQUIRE_NOTHROW(rtpCapabilities = mediasoupclient::Handler::GetNativeRtpCapabilities());

		REQUIRE(rtpCapabilities["codecs"].is_array());
		REQUIRE(rtpCapabilities["fecMechanisms"].is_array());
		REQUIRE(rtpCapabilities["headerExtensions"].is_array());
	}
}

TEST_CASE("SendHandler", "[Handler][SendHandler]")
{
	static FakeHandlerListener handlerListener;

	static mediasoupclient::SendHandler sendHandler(
	  &handlerListener,
	  TransportRemoteParameters["iceParameters"],
	  TransportRemoteParameters["iceCandidates"],
	  TransportRemoteParameters["dtlsParameters"],
	  TransportRemoteParameters["sctpParameters"],
	  &PeerConnectionOptions,
	  RtpParametersByKind,
	  RtpParametersByKind);

	static std::unique_ptr<mediasoupclient::PeerConnection> pc(
	  new mediasoupclient::PeerConnection(nullptr, &PeerConnectionOptions));

	static rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

	static std::string localId;

	SECTION("sendHandler.Send() fails if a null track is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.Send(nullptr, nullptr, nullptr, nullptr), MediaSoupClientError);
	}

	SECTION("sendHandler.Send() succeeds if a track is provided")
	{
		track = createAudioTrack("test-track-id");

		mediasoupclient::SendHandler::SendResult sendResult;

		REQUIRE_NOTHROW(sendResult = sendHandler.Send(track.get(), nullptr, nullptr, nullptr));

		localId = sendResult.localId;

		REQUIRE(sendResult.rtpParameters["codecs"].size() == 1);
		REQUIRE(sendResult.rtpParameters["headerExtensions"].size() == 3);
	}

	SECTION("sendHandler.Send() succeeds if track is already handled")
	{
		REQUIRE_NOTHROW(sendHandler.Send(track.get(), nullptr, nullptr, nullptr));
	}

	SECTION("sendHandler.ReplaceTrack() fails if an invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.ReplaceTrack("", nullptr), MediaSoupClientError);
	}

	SECTION("sendHandler.ReplaceTrack() succeeds if a new track is provided")
	{
		auto newTrack = createAudioTrack("test-new-track-id");

		REQUIRE_NOTHROW(sendHandler.ReplaceTrack(localId, newTrack.get()));

		track = newTrack;
	}

	SECTION("sendHandler.SetMaxSpatialLayer() fails if invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.SetMaxSpatialLayer("", 1), MediaSoupClientError);
	}

	SECTION("sendHandler.SetMaxSpatialLayer() succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.SetMaxSpatialLayer(localId, 1));
	}

	SECTION("sendHandler.GetSenderStats() fails if invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.GetSenderStats(""), MediaSoupClientError);
	}

	SECTION("sendHandler.GetSenderStats() succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.GetSenderStats(localId));
	}

	SECTION("sendHandler.StopSending() fails if an invalid localId is provided")
	{
		REQUIRE_THROWS_AS(sendHandler.StopSending(""), MediaSoupClientError);
	}

	SECTION("sendHandler.Sends() succeeds after stopping if track if provided")
	{
		mediasoupclient::SendHandler::SendResult sendResult;

		REQUIRE_NOTHROW(sendResult = sendHandler.Send(track.get(), nullptr, nullptr, nullptr));

		localId = sendResult.localId;
	}

	SECTION("sendHandler.StopSending() succeeds if track is being sent")
	{
		REQUIRE_NOTHROW(sendHandler.StopSending(localId));
	}

	SECTION("sendHandler.RestartIce() succeeds")
	{
		auto iceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendHandler.RestartIce(iceParameters));
	}

	SECTION("sendHandler.UpdateIceServers() succeeds")
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

	static mediasoupclient::RecvHandler recvHandler(
	  &handlerListener,
	  TransportRemoteParameters["iceParameters"],
	  TransportRemoteParameters["iceCandidates"],
	  TransportRemoteParameters["dtlsParameters"],
	  TransportRemoteParameters["sctpParameters"],
	  &PeerConnectionOptions);

	SECTION("recvHander.Receive() succeeds if correct rtpParameters are provided")
	{
		mediasoupclient::RecvHandler::RecvResult recvResult;

		REQUIRE_NOTHROW(recvResult = recvHandler.Receive("test", "audio", &rtpParameters));

		localId = recvResult.localId;
	}

	SECTION("recvHandler.GetReceiverStats() fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.GetReceiverStats("unknown"), MediaSoupClientError);
	}

	SECTION("recvHandler.GetReceiverStats() succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.GetReceiverStats(localId));
	}

	SECTION("recvHandler.StopReceiving() fails if unknown receiver id is provided")
	{
		REQUIRE_THROWS_AS(recvHandler.StopReceiving("unknown"), MediaSoupClientError);
	}

	SECTION("recvHandler.StopReceiving() succeeds if known receiver id is provided")
	{
		REQUIRE_NOTHROW(recvHandler.StopReceiving(localId));
	}

	SECTION("recvHandler.RestartIce() succeeds")
	{
		auto iceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvHandler.RestartIce(iceParameters));
	}

	SECTION("recvHandler.UpdateIceServers() succeeds")
	{
		REQUIRE_NOTHROW(recvHandler.UpdateIceServers(json::array()));
	}
}
