#include "Exception.hpp"
#include "PeerConnection.hpp"
#include "catch.hpp"
#include "helpers.hpp"
#include "sdp/Utils.hpp"

using namespace mediasoupclient;

TEST_CASE("PeerConnection", "[PeerConnection]")
{
	static std::list<std::string> iceServerUris;
	static PeerConnection::PrivateListener listener;
	static PeerConnection::Options peerConnectionOptions;
	static PeerConnection pc(&listener, &peerConnectionOptions);

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

		PeerConnection::PrivateListener listener;
		PeerConnection pc(&listener, &peerConnectionOptions);

		REQUIRE(!pc.SetConfiguration(configuration));
	}

	SECTION("'pc.GetStats()' succeeds")
	{
		REQUIRE_NOTHROW(pc.GetStats());
	}

	SECTION("'pc.CreateAnswer()' fails if no remote offer has been provided")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_THROWS_AS(pc.CreateAnswer(options), Exception);
	}

	SECTION("'pc.SetRemoteDescription()' fails if incorrect SDP is provided")
	{
		auto sdp = std::string();

		REQUIRE_THROWS_AS(pc.SetLocalDescription(PeerConnection::SdpType::OFFER, sdp), Exception);
	}

	SECTION("'pc.SetRemoteDescription()' succeeds if correct SDP is provided")
	{
		auto sdp = helpers::readFile("test/sdp/data/webrtc.sdp");

		REQUIRE_NOTHROW(pc.SetRemoteDescription(PeerConnection::SdpType::OFFER, sdp));
	}

	SECTION("'pc.CreateOffer()' succeeds")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_NOTHROW(offer = pc.CreateOffer(options));
	}

	SECTION("'pc.SetRemoteDescription()' succeeds")
	{
		REQUIRE_NOTHROW(pc.SetRemoteDescription(PeerConnection::SdpType::OFFER, offer));
	}

	SECTION("'pc.CreateAnswer()' succeeds if remote offer is provided")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		REQUIRE_NOTHROW(pc.CreateAnswer(options));
	}
}
