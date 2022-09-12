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
	static mediasoupclient::PeerConnection pc(&listener, peerConnectionOptions);

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
		mediasoupclient::PeerConnection pc(&listener, peerConnectionOptions);

		REQUIRE(!pc.SetConfiguration(configuration));
	}

	SECTION("'pc.GetStats()' succeeds")
	{
		std::promise<mediasoupclient::PeerConnection::RTCStatsReport> promise;
		pc.GetStats([&promise](auto v, auto) { promise.set_value(v); });

		auto stats = promise.get_future().get();
		REQUIRE(stats);
	}

	SECTION("'pc.CreateAnswer()' fails if no remote offer has been provided")
	{
		std::promise<webrtc::RTCError> promise;
		pc.CreateAnswer([&promise](auto, auto err) { promise.set_value(err); });

		auto error = promise.get_future().get();
		REQUIRE(!error.ok());
	}

	SECTION("'pc.SetRemoteDescription()' fails if incorrect SDP is provided")
	{
		std::promise<webrtc::RTCError> promise;
		
		pc.SetRemoteDescription(mediasoupclient::PeerConnection::SdpType::OFFER, {}, [&promise](auto, auto err) {
			promise.set_value(err);
		});

		auto error = promise.get_future().get();
		REQUIRE(!error.ok());
	}

	SECTION("'pc.SetRemoteDescription()' succeeds if correct SDP is provided")
	{
		auto sdp = helpers::readFile("test/data/webrtc.sdp");

		std::promise<webrtc::RTCError> promise;
		
		pc.SetRemoteDescription(mediasoupclient::PeerConnection::SdpType::OFFER, sdp, [&promise](auto, auto err) {
			promise.set_value(err);
		});

		auto error = promise.get_future().get();
		REQUIRE(error.ok());
	}

	SECTION("'pc.CreateOffer()' succeeds")
	{
		std::promise<std::string> promise;
		pc.CreateOffer([&promise](auto sdp, auto) { promise.set_value(sdp); });

		offer = promise.get_future().get();
		REQUIRE(!offer.empty());
	}

	SECTION("'pc.SetRemoteDescription()' succeeds")
	{
		std::promise<webrtc::RTCError> promise;
		pc.SetRemoteDescription(mediasoupclient::PeerConnection::SdpType::OFFER, offer, [&promise](auto, auto err) {
			promise.set_value(err);
		});

		auto error = promise.get_future().get();
		REQUIRE(error.ok());
	}

	SECTION("'pc.CreateAnswer()' succeeds if remote offer is provided")
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		std::promise<webrtc::RTCError> promise;
		pc.CreateAnswer([&promise](auto, auto err) { promise.set_value(err); });

		auto error = promise.get_future().get();
		REQUIRE(error.ok());
	}
}
