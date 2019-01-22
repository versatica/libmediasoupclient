#include "Exception.hpp"
#include "PeerConnection.hpp"
#include "catch.hpp"
#include "helpers.hpp"
#include "sdp/Utils.hpp"

using namespace mediasoupclient;

TEST_CASE("PeerConnection", "[PeerConnection]")
{
	static std::list<std::string> iceServerUris;
	static PeerConnection::Listener listener;
	static PeerConnection pc(&listener, iceServerUris);

	static std::string offer;

	SECTION("'pc.GetConfiguration()' succeeds")
	{
		auto configuration = pc.GetConfiguration();
	}

	SECTION("'pc.GetNativeRtpCapabilities()' succeeds")
	{
		auto capabilities = pc.GetNativeRtpCapabilities();

		REQUIRE(capabilities.at("codecs").is_array());

		auto& codecs = capabilities.at("codecs");

		REQUIRE(codecs.size());

		REQUIRE_NOTHROW(codecs[0].at("channels").get<uint8_t>());
		REQUIRE_NOTHROW(codecs[0].at("clockRate").get<uint16_t>());
		REQUIRE_NOTHROW(codecs[0].at("kind").get<std::string>());
		REQUIRE_NOTHROW(codecs[0].at("mimeType").get<std::string>());
		REQUIRE_NOTHROW(codecs[0].at("name").get<std::string>());
		REQUIRE_NOTHROW(codecs[0].at("preferredPayloadType").get<uint8_t>());
		REQUIRE_NOTHROW(codecs[0].at("parameters").is_object());
		REQUIRE_NOTHROW(codecs[0].at("rtcpFeedback").is_array());
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

		PeerConnection::Listener listener;
		PeerConnection pc(&listener, iceServerUris);

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
