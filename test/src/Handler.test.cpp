#include "Handler.hpp"
#include "MediaSoupClientErrors.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "fakeParameters.hpp"
#include "ortc.hpp"
#include <catch.hpp>

namespace
{
	class FakeHandlerListener : public mediasoupclient::Handler::PrivateListener
	{
	public:
		void OnConnect(json& /*transportLocalParameters*/) override {};

		void OnConnectionStateChange(
		  webrtc::PeerConnectionInterface::IceConnectionState /*connectionState*/) override{};
	};
} // namespace

TEST_CASE("Handler", "[Handler]")
{
	const MediaStreamTrackFactory& singleton = MediaStreamTrackFactory::GetInstance();
	SECTION("Handler::GetNativeRtpCapabilities() succeeds")
	{
		json rtpCapabilities;

		REQUIRE_NOTHROW(
		  rtpCapabilities =
		    mediasoupclient::Handler::GetNativeRtpCapabilities(&singleton.peerConnectionOptions));

		REQUIRE(rtpCapabilities["codecs"].is_array());
		REQUIRE(rtpCapabilities["fecMechanisms"].is_array());
		REQUIRE(rtpCapabilities["headerExtensions"].is_array());
	}
}

TEST_CASE("SendHandler", "[Handler][SendHandler]")
{
	const json transportRemoteParameters = generateTransportRemoteParameters();

	// NOTE: Captureless, since the handler below outlives this body (Catch2 runs
	// it once per SECTION) and copying a captured json may throw.
	const auto getSendCapabilities = [](json& currentLocalRtpCapabilities) -> json
	{
		auto routerRtpCapabilities = generateRouterRtpCapabilities();

		return mediasoupclient::ortc::getExtendedRtpCapabilities(
		  currentLocalRtpCapabilities, routerRtpCapabilities);
	};

	const MediaStreamTrackFactory& singleton = MediaStreamTrackFactory::GetInstance();

	static FakeHandlerListener handlerListener;

	static mediasoupclient::SendHandler sendHandler(
	  std::addressof(handlerListener),
	  transportRemoteParameters["iceParameters"],
	  transportRemoteParameters["iceCandidates"],
	  transportRemoteParameters["dtlsParameters"],
	  transportRemoteParameters["sctpParameters"],
	  &singleton.peerConnectionOptions,
	  getSendCapabilities);

	static webrtc::scoped_refptr<webrtc::AudioTrackInterface> track;

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
		const auto newTrack = createAudioTrack("test-new-track-id");

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
		const auto& iceParameters = transportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendHandler.RestartIce(iceParameters));
	}

	SECTION("sendHandler.UpdateIceServers() succeeds")
	{
		REQUIRE_NOTHROW(sendHandler.UpdateIceServers(json::array()));
		sendHandler.Close();
	}
}

TEST_CASE("RecvHandler", "[Handler][RecvHandler]")
{
	auto consumerRemoteParameters = generateConsumerRemoteParameters("audio/opus");

	const auto rtpParameters             = consumerRemoteParameters["rtpParameters"];
	const json transportRemoteParameters = generateTransportRemoteParameters();

	static std::string localId;

	const MediaStreamTrackFactory& singleton = MediaStreamTrackFactory::GetInstance();

	static FakeHandlerListener handlerListener;

	static mediasoupclient::RecvHandler recvHandler(
	  std::addressof(handlerListener),
	  transportRemoteParameters["iceParameters"],
	  transportRemoteParameters["iceCandidates"],
	  transportRemoteParameters["dtlsParameters"],
	  transportRemoteParameters["sctpParameters"],
	  &singleton.peerConnectionOptions);

	SECTION("recvHander.Receive() succeeds if correct rtpParameters are provided")
	{
		mediasoupclient::RecvHandler::RecvResult recvResult;

		REQUIRE_NOTHROW(recvResult = recvHandler.Receive("test", "audio", std::addressof(rtpParameters)));

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
		const auto& iceParameters = transportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(recvHandler.RestartIce(iceParameters));
	}

	SECTION("recvHandler.UpdateIceServers() succeeds")
	{
		REQUIRE_NOTHROW(recvHandler.UpdateIceServers(json::array()));
		recvHandler.Close();
	}
}
