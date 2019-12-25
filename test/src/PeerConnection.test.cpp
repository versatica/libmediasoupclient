#include "MediaSoupClientErrors.hpp"
#include "PeerConnection.hpp"
#include "helpers.hpp"
#include "sdp/Utils.hpp"
#include <catch.hpp>

TEST_CASE("PeerConnection", "[PeerConnection]")
{
	static std::list<std::string> iceServerUris;
	static mediasoupclient::PeerConnection::PrivateListener listener;
	static mediasoupclient::PeerConnection::Options peerConnectionOptions;
	static mediasoupclient::PeerConnection pc(&listener, &peerConnectionOptions);

	static std::string offer;

	SECTION("'pc.GetConfiguration()' succeeds")
	{
		auto configuration = pc.GetConfiguration();
	}

	/*
	 * NOTE: Fails if peerconnection is created with Unified Plan SDP semantics.
	 * See: src/PeerConnection.cpp (constructor).
	SECTION("SetConfiguration (right options)")
	{
	  webrtc::PeerConnectionInterface::RTCConfiguration configuration;

	  REQUIRE(pc.SetConfiguration(configuration) == true);
	}
	*/

	SECTION("'pc.SetConfiguration()' fails if wrong options are provided")
	{
		webrtc::PeerConnectionInterface::RTCConfiguration configuration;
		webrtc::PeerConnectionInterface::IceServer iceServer;

		iceServer.uri = "Wrong URI";
		configuration.servers.push_back(iceServer);

		mediasoupclient::PeerConnection::PrivateListener listener;
		mediasoupclient::PeerConnection pc(&listener, &peerConnectionOptions);

		REQUIRE(!pc.SetConfiguration(configuration));
	}

	SECTION("'pc.GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(pc.GetStats());
	}

	SECTION("'pc.CreateAnswer()' fails if no remote offer has been provided")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_THROWS_AS(pc.CreateAnswer(options), MediaSoupClientError);
	}

	SECTION("'pc.SetRemoteDescription()' fails if incorrect SDP is provided")
	{
		auto sdp = std::string();

		REQUIRE_THROWS_AS(
		  pc.SetLocalDescription(mediasoupclient::PeerConnection::SdpType::OFFER, sdp),
		  MediaSoupClientError);
	}

	SECTION("'pc.SetRemoteDescription()' succeeds if correct SDP is provided")
	{
		auto sdp = helpers::readFile("test/data/webrtc.sdp");

		REQUIRE_NOTHROW(pc.SetRemoteDescription(mediasoupclient::PeerConnection::SdpType::OFFER, sdp));
	}

	SECTION("'pc.CreateOffer()' succeeds")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_NOTHROW(offer = pc.CreateOffer(options));
	}

	SECTION("'pc.SetRemoteDescription()' succeeds")
	{
		REQUIRE_NOTHROW(pc.SetRemoteDescription(mediasoupclient::PeerConnection::SdpType::OFFER, offer));
	}

	SECTION("'pc.CreateAnswer()' succeeds if remote offer is provided")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_NOTHROW(pc.CreateAnswer(options));
	}
}
