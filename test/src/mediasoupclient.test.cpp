#include "FakeTransportListener.hpp"
#include "MediaSoupClientErrors.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "fakeParameters.hpp"
#include "mediasoupclient.hpp"
#include <catch.hpp>
#include <vector>

TEST_CASE("mediasoupclient", "[mediasoupclient]")
{
	static const json TransportRemoteParameters = generateTransportRemoteParameters();

	static FakeSendTransportListener sendTransportListener;
	static FakeRecvTransportListener recvTransportListener;

	static std::unique_ptr<mediasoupclient::Device> device;
	static std::unique_ptr<mediasoupclient::SendTransport> sendTransport;
	static std::unique_ptr<mediasoupclient::SendTransport> sendTransportNoSctp;
	static std::unique_ptr<mediasoupclient::RecvTransport> recvTransport;
	static std::unique_ptr<mediasoupclient::Producer> audioProducer;
	static std::unique_ptr<mediasoupclient::Producer> videoProducer;
	static std::unique_ptr<mediasoupclient::Consumer> audioConsumer;
	static std::unique_ptr<mediasoupclient::Consumer> videoConsumer;
	static std::unique_ptr<mediasoupclient::Consumer> audioConsumer2;

	static std::unique_ptr<mediasoupclient::DataProducer> dataProducer;
	static std::unique_ptr<mediasoupclient::DataConsumer> dataConsumer;

	static rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack;
	static rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack;

	static FakeProducerListener producerListener;
	static FakeConsumerListener consumerListener;

	static json routerRtpCapabilities;

	SECTION("create a Device succeeds")
	{
		REQUIRE_NOTHROW(device.reset(new mediasoupclient::Device()));
		REQUIRE(!device->IsLoaded());
	}

	SECTION("device.GetRtpCapabilities throws if not loaded")
	{
		REQUIRE_THROWS_AS(device->GetRtpCapabilities(), MediaSoupClientInvalidStateError);
	}

	SECTION("device.CanProduce() throws if not loaded")
	{
		REQUIRE_THROWS_AS(device->CanProduce("audio"), MediaSoupClientInvalidStateError);
		REQUIRE_THROWS_AS(device->CanProduce("video"), MediaSoupClientInvalidStateError);
	}

	SECTION("device.CreateSendTransport() throws if not loaded")
	{
		REQUIRE_THROWS_AS(
		  device->CreateSendTransport(
		    &sendTransportListener,
		    TransportRemoteParameters["id"],
		    TransportRemoteParameters["iceParameters"],
		    TransportRemoteParameters["iceCandidates"],
		    TransportRemoteParameters["dtlsParameters"],
		    nullptr),
		  MediaSoupClientInvalidStateError);
	}

	SECTION("device.Load() with invalid routerRtpCapabilities rejects with TypeError")
	{
		auto routerRtpCapabilities = R"(
		{
			"codecs" : {}
		})"_json;

		REQUIRE_THROWS_AS(device->Load(routerRtpCapabilities), MediaSoupClientTypeError);
		REQUIRE(!device->IsLoaded());
	}

	SECTION("device.load() with invalid routerRtpCapabilities throws")
	{
		routerRtpCapabilities = generateRouterRtpCapabilities();

		for (auto& codec : routerRtpCapabilities["codecs"])
		{
			codec.erase("mimeType");
		}

		REQUIRE_THROWS_AS(device->Load(routerRtpCapabilities), MediaSoupClientTypeError);
	}

	SECTION("device.load() succeeds")
	{
		routerRtpCapabilities = generateRouterRtpCapabilities();

		REQUIRE_NOTHROW(device->Load(routerRtpCapabilities));
		REQUIRE(device->IsLoaded());
	}

	SECTION("device.load() rejects if already loaded")
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
		REQUIRE_THROWS_AS(device->CanProduce("chicken"), MediaSoupClientTypeError);
	}

	SECTION("device.createSendTransport() for sending media succeeds")
	{
		/* clang-format off */
		json appData =
		{
			{ "baz", "BAZ" }
		};
		/* clang-format on */

		REQUIRE_NOTHROW(sendTransport.reset(device->CreateSendTransport(
		  &sendTransportListener,
		  TransportRemoteParameters["id"],
		  TransportRemoteParameters["iceParameters"],
		  TransportRemoteParameters["iceCandidates"],
		  TransportRemoteParameters["dtlsParameters"],
		  TransportRemoteParameters["sctpParameters"],
		  nullptr,
		  appData)));

		REQUIRE(sendTransport->GetId() == TransportRemoteParameters["id"].get<std::string>());
		REQUIRE(!sendTransport->IsClosed());
		REQUIRE(sendTransport->GetConnectionState() == "new");
		REQUIRE(sendTransport->GetAppData() == appData);
	}

	SECTION("device.createRecvTransport() for receiving media succeeds")
	{
		REQUIRE_NOTHROW(recvTransport.reset(device->CreateRecvTransport(
		  &recvTransportListener,
		  TransportRemoteParameters["id"],
		  TransportRemoteParameters["iceParameters"],
		  TransportRemoteParameters["iceCandidates"],
		  TransportRemoteParameters["dtlsParameters"],
		  nullptr)));

		REQUIRE(recvTransport->GetId() == TransportRemoteParameters["id"].get<std::string>());
		REQUIRE(!recvTransport->IsClosed());
		REQUIRE(recvTransport->GetConnectionState() == "new");
		REQUIRE(recvTransport->GetAppData().empty());
	}

	SECTION("transport.produce() succeeds")
	{
		/* clang-format off */
		json appData =
		{
			{ "baz", "BAZ" }
		};
		/* clang-format on */

		std::vector<webrtc::RtpEncodingParameters> encodings;

		encodings.emplace_back(webrtc::RtpEncodingParameters());
		encodings.emplace_back(webrtc::RtpEncodingParameters());
		encodings.emplace_back(webrtc::RtpEncodingParameters());

		audioTrack = createAudioTrack("audio-track-id");
		videoTrack = createVideoTrack("video-track-id");

		json codecs;
		json headerExtensions;
		json rtcp;

		// Pause the audio track before creating its Producer.
		audioTrack->set_enabled(false);

		/* clang-format off */
		json codecOptions =
		{
			{ "opusStereo", true },
			{ "opusDtx",    true }
		};
		/* clang-format on */

		REQUIRE_NOTHROW(audioProducer.reset(sendTransport->Produce(
		  &producerListener, audioTrack, nullptr, &codecOptions, nullptr, appData)));

		REQUIRE(
		  sendTransportListener.onConnectTimesCalled ==
		  ++sendTransportListener.onConnectExpectedTimesCalled);

		REQUIRE(sendTransportListener.id == sendTransport->GetId());

		REQUIRE(
		  sendTransportListener.onProduceExpectedTimesCalled ==
		  ++sendTransportListener.onProduceExpectedTimesCalled);

		REQUIRE(sendTransportListener.appData == appData);

		REQUIRE(audioProducer->GetId() == sendTransportListener.audioProducerId);
		REQUIRE(!audioProducer->IsClosed());
		REQUIRE(audioProducer->GetKind() == "audio");
		REQUIRE(audioProducer->GetRtpSender() != nullptr);
		REQUIRE(audioProducer->GetTrack() == audioTrack);
		REQUIRE(audioProducer->IsPaused());
		REQUIRE(audioProducer->GetMaxSpatialLayer() == 0);
		REQUIRE(audioProducer->GetAppData() == appData);
		REQUIRE(audioProducer->GetRtpParameters()["codecs"].size() == 1);

		codecs = audioProducer->GetRtpParameters()["codecs"];
		REQUIRE(codecs[0].is_object());

		headerExtensions = audioProducer->GetRtpParameters()["headerExtensions"];
		REQUIRE(headerExtensions.is_array());

		auto enc = audioProducer->GetRtpParameters()["encodings"];
		REQUIRE(enc.is_array());
		REQUIRE(enc.size() == 1);
		REQUIRE(enc[0].is_object());
		REQUIRE(enc[0].find("ssrc") != enc[0].end());
		REQUIRE(enc[0]["ssrc"].is_number());

		rtcp = audioProducer->GetRtpParameters()["rtcp"];
		REQUIRE(rtcp.is_object());
		REQUIRE(rtcp["cname"].is_string());

		audioProducer->Resume();

		REQUIRE_NOTHROW(videoProducer.reset(
		  sendTransport->Produce(&producerListener, videoTrack, &encodings, nullptr, nullptr)));

		REQUIRE(
		  sendTransportListener.onConnectTimesCalled ==
		  sendTransportListener.onConnectExpectedTimesCalled);

		REQUIRE(
		  sendTransportListener.onProduceExpectedTimesCalled ==
		  ++sendTransportListener.onProduceExpectedTimesCalled);

		REQUIRE(videoProducer->GetId() == sendTransportListener.videoProducerId);
		REQUIRE(!videoProducer->IsClosed());
		REQUIRE(videoProducer->GetKind() == "video");
		REQUIRE(videoProducer->GetRtpSender() != nullptr);
		REQUIRE(videoProducer->GetTrack() == videoTrack);
		REQUIRE(!videoProducer->GetRtpParameters()["codecs"].empty());

		codecs = videoProducer->GetRtpParameters()["codecs"];
		REQUIRE(codecs[0].is_object());

		headerExtensions = videoProducer->GetRtpParameters()["headerExtensions"];
		REQUIRE(headerExtensions.is_array());

		enc = videoProducer->GetRtpParameters()["encodings"];
		REQUIRE(enc.is_array());
		REQUIRE(enc.size() == 3);
		REQUIRE(enc[0].is_object());
		REQUIRE(enc[0].find("rid") != enc[0].end());
		REQUIRE(enc[0]["rid"] == "r0");

		rtcp = videoProducer->GetRtpParameters()["rtcp"];
		REQUIRE(rtcp.is_object());
		REQUIRE(rtcp["cname"].is_string());

		videoProducer->SetMaxSpatialLayer(2);
		REQUIRE(!videoProducer->IsPaused());
		REQUIRE(videoProducer->GetMaxSpatialLayer() == 2);
		REQUIRE(videoProducer->GetAppData() == json::object());
	}

	SECTION("transport.produceData() succeeds")
	{
		/* clang-format off */
		json appData =
		{
			{ "tdr", "TDR" }
		};
		/* clang-format on */

		REQUIRE_NOTHROW(dataProducer.reset(
		  sendTransport->ProduceData(&producerListener, "", "", true, 0, 0, appData)));

		REQUIRE(
		  sendTransportListener.onConnectTimesCalled ==
		  sendTransportListener.onConnectExpectedTimesCalled); // connect has already been called for Producer

		REQUIRE(sendTransportListener.id == sendTransport->GetId());

		REQUIRE(
		  sendTransportListener.onProduceDataExpectedTimesCalled ==
		  ++sendTransportListener.onProduceDataExpectedTimesCalled);

		REQUIRE(dataProducer->GetId() == sendTransportListener.dataProducerId);
		REQUIRE(!dataProducer->IsClosed());
		REQUIRE(dataProducer->GetAppData() == appData);
	}

	SECTION("transport.produce() without track throws")
	{
		REQUIRE_THROWS_AS(
		  sendTransport->Produce(&producerListener, nullptr, nullptr, nullptr, nullptr),
		  MediaSoupClientTypeError);
	}

	SECTION("transport.produceData() on transport without sctpParameters throws")
	{
		/* clang-format off */
		json appData =
		{
			{ "tdr", "TDR" }
		};
		/* clang-format on */

		REQUIRE_NOTHROW(sendTransportNoSctp.reset(device->CreateSendTransport(
		  &sendTransportListener,
		  TransportRemoteParameters["id"],
		  TransportRemoteParameters["iceParameters"],
		  TransportRemoteParameters["iceCandidates"],
		  TransportRemoteParameters["dtlsParameters"],
		  nullptr,
		  appData)));

		REQUIRE_THROWS_AS(
		  sendTransportNoSctp->ProduceData(&producerListener, "", "", true, 0, 0, appData),
		  MediaSoupClientError);
	}

	SECTION("transport.consume() succeeds")
	{
		/* clang-format off */
		json appData =
		{
			{ "baz", "BAZ" }
		};

		auto audioConsumerRemoteParameters =
			generateConsumerRemoteParameters("audio/opus");
		auto videoConsumerRemoteParameters =
			generateConsumerRemoteParameters("video/VP8");
		auto audioConsumer2RemoteParameters =
			generateConsumerRemoteParameters("audio/opus");

		json codecs;
		json headerExtensions;
		json encodings;
		json rtcp;

		REQUIRE_NOTHROW(audioConsumer.reset(
			recvTransport->Consume(
				&consumerListener,
				audioConsumerRemoteParameters["id"].get<std::string>(),
				audioConsumerRemoteParameters["producerId"].get<std::string>(),
				audioConsumerRemoteParameters["kind"].get<std::string>(),
				&audioConsumerRemoteParameters["rtpParameters"],
				appData)));

		REQUIRE(
		  recvTransportListener.onConnectTimesCalled == ++recvTransportListener.onConnectExpectedTimesCalled);

		REQUIRE(recvTransportListener.id == recvTransport->GetId());
		REQUIRE(recvTransportListener.dtlsParameters.is_object());

		REQUIRE(audioConsumer->GetId() == audioConsumerRemoteParameters["id"].get<std::string>());
		REQUIRE(audioConsumer->GetProducerId() == audioConsumerRemoteParameters["producerId"].get<std::string>());

		REQUIRE(!audioConsumer->IsClosed());
		REQUIRE(audioConsumer->GetKind() == "audio");
		REQUIRE(audioConsumer->GetRtpReceiver() != nullptr);
		REQUIRE(audioConsumer->GetRtpParameters()["codecs"].is_array());
		REQUIRE(audioConsumer->GetRtpParameters()["codecs"].size() == 1);

		codecs = audioConsumer->GetRtpParameters()["codecs"];

		REQUIRE(codecs[0] == R"(
		{
			"channels":    2,
			"clockRate":   48000,
			"mimeType":    "audio/opus",
			"parameters":
			{
				"useinbandfec": "1"
			},
			"payloadType":  100,
			"rtcpFeedback": []
		})"_json);

		headerExtensions = audioConsumer->GetRtpParameters()["headerExtensions"];
		REQUIRE(headerExtensions == R"(
		[
			{
				"id":  1,
				"uri": "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
				"encrypt": false,
				"parameters": {}
			}
		])"_json);

		encodings = audioConsumer->GetRtpParameters()["encodings"];
		REQUIRE(encodings.is_array());
		REQUIRE(encodings.size() == 1);
		REQUIRE(encodings[0].is_object());
		REQUIRE(encodings[0].find("ssrc") != encodings[0].end());
		REQUIRE(encodings[0]["ssrc"].is_number());

		rtcp = audioConsumer->GetRtpParameters()["rtcp"];
		REQUIRE(rtcp.is_object());
		REQUIRE(rtcp["cname"].is_string());

		REQUIRE(!audioConsumer->IsPaused());
		REQUIRE(audioConsumer->GetAppData() == appData);

		REQUIRE_NOTHROW(videoConsumer.reset(
			recvTransport->Consume(
				&consumerListener,
				videoConsumerRemoteParameters["id"].get<std::string>(),
				videoConsumerRemoteParameters["producerId"].get<std::string>(),
				videoConsumerRemoteParameters["kind"].get<std::string>(),
				&videoConsumerRemoteParameters["rtpParameters"])));

		REQUIRE(
		  recvTransportListener.onConnectTimesCalled == recvTransportListener.onConnectExpectedTimesCalled);

		REQUIRE(videoConsumer->GetId() == videoConsumerRemoteParameters["id"].get<std::string>());
		REQUIRE(videoConsumer->GetProducerId() == videoConsumerRemoteParameters["producerId"].get<std::string>());

		REQUIRE(!videoConsumer->IsClosed());
		REQUIRE(videoConsumer->GetKind() == "video");
		REQUIRE(videoConsumer->GetRtpReceiver() != nullptr);
		REQUIRE(videoConsumer->GetRtpParameters()["codecs"].is_array());
		REQUIRE(videoConsumer->GetRtpParameters()["codecs"].size() == 2);

		codecs = videoConsumer->GetRtpParameters()["codecs"];

		REQUIRE(codecs[0] == R"(
		{
			"clockRate": 90000,
			"mimeType": "video/VP8",
			"parameters":
			{
				"x-google-start-bitrate": "1500"
			},
			"payloadType":  101,
			"rtcpFeedback":
			[
				{
				  "type": "nack",
				  "parameter": ""
				},
				{
					"type": "nack",
				  "parameter" : "pli"
				},
				{
				  "type": "nack",
				  "parameter": "sli"
				},
				{
					"type": "nack",
				  "parameter": "rpsi"
				},
				{
					"type": "nack",
				  "parameter": "app"
				},
				{
					"type": "ccm",
				  "parameter": "fir"
				},
				{
				  "type": "goog-remb",
				  "parameter": ""
				}
			]
		})"_json);

		REQUIRE(codecs[1] == R"(
		{
			"clockRate": 90000,
			"mimeType": "video/rtx",
			"parameters":
			{
			  "apt": 101
			},
			"payloadType": 102,
			"rtcpFeedback": []
		})"_json);

		headerExtensions = videoConsumer->GetRtpParameters()["headerExtensions"];
		REQUIRE(headerExtensions == R"(
		[
			{
			  "encrypt": false,
			  "id": 2,
			  "uri": "urn:ietf:params:rtp-hdrext:toffset",
			  "parameters": {}
			},
			{
			  "encrypt": false,
			  "id": 3,
			  "uri": "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
			  "parameters": {}
			}
		])"_json);

		encodings = videoConsumer->GetRtpParameters()["encodings"];
		REQUIRE(encodings.is_array());
		REQUIRE(encodings.size() == 1);
		REQUIRE(encodings[0].is_object());
		REQUIRE(encodings[0].find("ssrc") != encodings[0].end());
		REQUIRE(encodings[0]["ssrc"].is_number());
		REQUIRE(encodings[0]["rtx"].is_object());
		REQUIRE(encodings[0]["rtx"].find("ssrc") != encodings[0]["rtx"].end());
		REQUIRE(encodings[0]["rtx"]["ssrc"].is_number());

		rtcp = videoConsumer->GetRtpParameters()["rtcp"];
		REQUIRE(rtcp.is_object());
		REQUIRE(rtcp["cname"].is_string());

		REQUIRE(!videoConsumer->IsPaused());
		REQUIRE(videoConsumer->GetAppData() == json::object());

		// Consume an additional audio track.
		REQUIRE_NOTHROW(audioConsumer2.reset(
			recvTransport->Consume(
				&consumerListener,
				audioConsumer2RemoteParameters["id"].get<std::string>(),
				audioConsumer2RemoteParameters["producerId"].get<std::string>(),
				audioConsumer2RemoteParameters["kind"].get<std::string>(),
				&audioConsumer2RemoteParameters["rtpParameters"],
				appData)));

		REQUIRE(
		  recvTransportListener.onConnectTimesCalled == recvTransportListener.onConnectExpectedTimesCalled);

		REQUIRE(recvTransportListener.id == recvTransport->GetId());
		REQUIRE(recvTransportListener.dtlsParameters.is_object());

		REQUIRE(audioConsumer2->GetId() == audioConsumer2RemoteParameters["id"].get<std::string>());
		REQUIRE(audioConsumer2->GetProducerId() == audioConsumer2RemoteParameters["producerId"].get<std::string>());

		REQUIRE(!audioConsumer2->IsClosed());
		REQUIRE(audioConsumer2->GetKind() == "audio");
		REQUIRE(audioConsumer2->GetRtpReceiver() != nullptr);
		REQUIRE(audioConsumer2->GetRtpParameters()["codecs"].is_array());
		REQUIRE(audioConsumer2->GetRtpParameters()["codecs"].size() == 1);

		codecs = audioConsumer2->GetRtpParameters()["codecs"];

		REQUIRE(codecs[0] == R"(
		{
			"channels": 2,
			"clockRate": 48000,
			"mimeType": "audio/opus",
			"parameters":
			{
				"useinbandfec": "1"
			},
			"payloadType": 100,
			"rtcpFeedback": []
		})"_json);

		headerExtensions = audioConsumer2->GetRtpParameters()["headerExtensions"];
		REQUIRE(headerExtensions == R"(
		[
			{
				"encrypt": false,
				"id": 1,
				"uri": "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
				"parameters": {}
			}
		])"_json);

		encodings = audioConsumer2->GetRtpParameters()["encodings"];
		REQUIRE(encodings.is_array());
		REQUIRE(encodings.size() == 1);
		REQUIRE(encodings[0].is_object());
		REQUIRE(encodings[0].find("ssrc") != encodings[0].end());
		REQUIRE(encodings[0]["ssrc"].is_number());

		rtcp = audioConsumer2->GetRtpParameters()["rtcp"];
		REQUIRE(rtcp.is_object());
		REQUIRE(rtcp["cname"].is_string());

		REQUIRE(!audioConsumer2->IsPaused());
		REQUIRE(audioConsumer2->GetAppData() == appData);
	}

	SECTION("transport.consume() with unsupported consumerRtpParameters throws")
	{
		auto consumerRemoteParameters =
			generateConsumerRemoteParameters("audio/ISAC");

		REQUIRE_THROWS_AS(
			recvTransport->Consume(
				&consumerListener,
				consumerRemoteParameters["id"].get<std::string>(),
				consumerRemoteParameters["producerId"].get<std::string>(),
				consumerRemoteParameters["kind"].get<std::string>(),
				&consumerRemoteParameters["rtpParameters"]),
			MediaSoupClientError);
	}

	SECTION("sendTransport.GetStats() succeeds")
	{
		REQUIRE_NOTHROW(sendTransport->GetStats());
	}

	SECTION("sendTransport.RestartIce() succeeds")
	{
		auto iceParameters = TransportRemoteParameters["iceParameters"];

		REQUIRE_NOTHROW(sendTransport->RestartIce(iceParameters));
	}

	SECTION("sendTransport.UpdateIceServers() succeeds")
	{
		auto iceServers = json::array();

		REQUIRE_NOTHROW(sendTransport->UpdateIceServers(iceServers));
	}

	SECTION("producer.Pause() succeeds")
	{
		videoProducer->Pause();

		REQUIRE(videoProducer->IsPaused());
	}

	SECTION("producer.Resume() succeeds")
	{
		videoProducer->Resume();

		REQUIRE(!videoProducer->IsPaused());
	}

	SECTION("producer.ReplaceTrack() succeeds")
	{
		// Have the audio Producer paused.
		audioProducer->Pause();

		auto newAudioTrack = createAudioTrack("audio-track-id-2");

		REQUIRE_NOTHROW(audioProducer->ReplaceTrack(newAudioTrack));
		REQUIRE(audioProducer->GetTrack() == newAudioTrack);
		// Producer was already paused.
		REQUIRE(audioProducer->IsPaused());

		// Reset the audio paused state.
		audioProducer->Resume();

		auto newVideoTrack = createVideoTrack("video-track-id-2");

		REQUIRE_NOTHROW(videoProducer->ReplaceTrack(newVideoTrack));
		REQUIRE(videoProducer->GetTrack() == newVideoTrack);
		REQUIRE(!videoProducer->IsPaused());

		videoTrack.release();
		videoTrack = newVideoTrack;
	}

	SECTION("producer.ReplaceTrack() fails if null track is provided")
	{
		REQUIRE_THROWS_AS(
			videoProducer->ReplaceTrack(nullptr),
			MediaSoupClientError);
	}

	SECTION("producer.SetMaxSpatialLayer() succeeds")
	{
		REQUIRE_NOTHROW(videoProducer->SetMaxSpatialLayer(1));
		REQUIRE(videoProducer->GetMaxSpatialLayer() == 1);
	}

	SECTION("producer.SetMaxSpatialLayer() in an audio Producer throws")
	{
		REQUIRE_THROWS_AS(
			audioProducer->SetMaxSpatialLayer(1),
			MediaSoupClientError);
	}

	SECTION("producer.GetStats() succeeds")
	{
		REQUIRE_NOTHROW(videoProducer->GetStats());
	}

	SECTION("consumer.Resume() succeeds")
	{
		videoConsumer->Resume();

		REQUIRE(!videoConsumer->IsPaused());
	}

	SECTION("consumer.Pause() succeeds")
	{
		videoConsumer->Pause();

		REQUIRE(videoConsumer->IsPaused());
	}

	SECTION("consumer.GetStats() succeeds")
	{
		REQUIRE_NOTHROW(videoConsumer->GetStats());
	}

	SECTION("producer.Close() succeeds")
	{
		audioProducer->Close();

		REQUIRE(audioProducer->IsClosed());
	}

	SECTION("producer.getStats() throws if closed")
	{
		REQUIRE_THROWS_AS(
			audioProducer->GetStats(),
			MediaSoupClientError);
	}

	SECTION("consumer.Close() succeeds")
	{
		audioConsumer->Close();

		REQUIRE(audioConsumer->IsClosed());
	}

	SECTION("consumer.getStats() throws if closed")
	{
		REQUIRE_THROWS_AS(
			audioConsumer->GetStats(),
			MediaSoupClientError);
	}

	SECTION("transport.Close() fires 'OnTransportClose' in live Producers/Consumers")
	{
		// Audio Producer was already closed.
		REQUIRE(audioProducer->IsClosed());
		REQUIRE(!videoProducer->IsClosed());

		sendTransport->Close();
		REQUIRE(sendTransport->IsClosed());
		REQUIRE(videoProducer->IsClosed());
		// Audio Producer was already closed.
		REQUIRE(producerListener.onTransportCloseTimesCalled == ++producerListener.onTransportCloseExpectedTimesCalled);

		// Audio Consumer was already closed.
		REQUIRE(audioConsumer->IsClosed());
		REQUIRE(!videoConsumer->IsClosed());
		REQUIRE(!audioConsumer2->IsClosed());

		recvTransport->Close();
		REQUIRE(audioConsumer->IsClosed());
		REQUIRE(videoConsumer->IsClosed());
		REQUIRE(audioConsumer2->IsClosed());
		// Audio Producer was already closed.
		REQUIRE(consumerListener.onTransportCloseTimesCalled == (consumerListener.onTransportCloseExpectedTimesCalled += 2));
	}

	SECTION("transport.produce() throws if closed")
	{
		REQUIRE_THROWS_AS(
			sendTransport->Produce(
				&producerListener,
				audioTrack, nullptr,  nullptr, nullptr),
			MediaSoupClientError);
	}

	SECTION("transport.consume() throws if closed")
	{
		auto audioConsumerRemoteParameters =
			generateConsumerRemoteParameters("audio/opus");

		REQUIRE_THROWS_AS(
			recvTransport->Consume(
				&consumerListener,
				audioConsumerRemoteParameters["id"].get<std::string>(),
				audioConsumerRemoteParameters["producerId"].get<std::string>(),
				audioConsumerRemoteParameters["kind"].get<std::string>(),
				&audioConsumerRemoteParameters["rtpParameters"]),
			MediaSoupClientError);
	}

	SECTION("transport.getStats() throws if closed")
	{
		REQUIRE_THROWS_AS(
			sendTransport->GetStats(),
			MediaSoupClientError);
	}

	SECTION("transport.restartIce() throws if closed")
	{
		auto iceParameters = json::object();

		REQUIRE_THROWS_AS(
			sendTransport->RestartIce(iceParameters),
			MediaSoupClientError);
	}

	SECTION("transport.restartIce() throws if closed")
	{
		auto iceServers = json::object();

		REQUIRE_THROWS_AS(
			sendTransport->UpdateIceServers(iceServers),
			MediaSoupClientError);
	}
}
