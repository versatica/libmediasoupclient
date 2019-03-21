#include "catch.hpp"
#include "helpers.hpp"
#include "sdp/RemoteSdp.hpp"
#include "sdptransform.hpp"

using namespace mediasoupclient;

TEST_CASE("SendRemoteSdp", "[SendRemoteSdp]")
{
	SECTION("audio only", "[CreateAnswerSdp]")
	{
		auto dtlsParameters = R"({
			"role"         : "client",
			"fingerprints" :
			[
				{
					"algorithm" : "sha-256",
					"value"     : "79:14:AB:AB:93:7F:07:E8:91:1A:11:16:36:D0:11:66:C4:4F:31:A0:74:46:65:58:70:E5:09:95:48:F4:4B:D9"
				}
			]
		})"_json;

		auto iceParameters = R"({
			"usernameFragment" : "5I2uVefP13X1wzOY",
			"password"         : "e46UjXntt0K/xTncQcDBQePn"
		})"_json;

		auto iceCandidates = R"([
			{
				"foundation" : "1162875081",
				"component"  : 1,
				"protocol"   : "udp",
				"priority"   : 2113937151,
				"ip"         : "192.168.34.75",
				"port"       : 60017,
				"type"       : "host",
				"generation" : 0
			}
		])"_json;

		auto audioCodecs = R"([
			{
				"mimeType"             : "audio/PCMU",
				"kind"                 : "audio",
				"clockRate"            : 8000,
				"payloadType"          : 0,
				"rtcpFeedback"         : [],
				"parameters"           : {}
			},
			{
				"mimeType"             : "audio/opus",
				"kind"                 : "audio",
				"clockRate"            : 48000,
				"payloadType"          : 96,
				"rtcpFeedback"         : [],
				"parameters"           :
				{
					"usedtx"       : "1",
					"useinbandfec" : "1"
				}
			}
		])"_json;

		auto headerExtensions = R"([
			{
				"value" : 1,
				"uri"   : "URI-toffset"
			}
		])"_json;

		/* clang-format off */
		json sendingRtpParametersByKind =
		{
			{
				"audio",
				{
					{ "codecs",           audioCodecs      },
					{ "headerExtensions", headerExtensions }
				}
			}
		};

		/* clang-format on */

		auto* remoteSdp = new Sdp::RemoteSdp(iceParameters, iceCandidates, dtlsParameters);

		auto sdp         = helpers::readFile("test/sdp/data/jssip.sdp");
		auto localSdpObj = sdptransform::parse(sdp);

		auto sdpAnswer = remoteSdp->GetSdp();
		auto parsed    = sdptransform::parse(sdpAnswer);

		REQUIRE(parsed.find("fingerprint") != parsed.end());
		REQUIRE(
		  parsed["fingerprint"]["hash"] ==
		  "79:14:AB:AB:93:7F:07:E8:91:1A:11:16:36:D0:11:66:C4:4F:31:A0:74:46:65:58:70:E5:09:95:48:F4:4B:D9");
		REQUIRE(parsed["fingerprint"]["type"] == "sha-256");

		REQUIRE(parsed.find("groups") != parsed.end());
		REQUIRE(parsed["groups"].size() == 1);
		// REQUIRE(parsed["groups"][0]["mids"] == "audio");
		REQUIRE(parsed["groups"][0]["type"] == "BUNDLE");

		// REQUIRE(parsed.find("media") != parsed.end());
		// REQUIRE(parsed["media"].size() == 1);

		// REQUIRE(parsed["media"][0].find("candidates") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["candidates"].size() == 1);
		// REQUIRE(parsed["media"][0]["candidates"][0]["component"] == 1);
		// REQUIRE(parsed["media"][0]["candidates"][0]["foundation"] == "1162875081");
		// REQUIRE(parsed["media"][0]["candidates"][0]["ip"] == "192.168.34.75");
		// REQUIRE(parsed["media"][0]["candidates"][0]["port"] == 60017);
		// REQUIRE(parsed["media"][0]["candidates"][0]["priority"] == 2113937151);
		// REQUIRE(parsed["media"][0]["candidates"][0]["transport"] == "udp");
		// REQUIRE(parsed["media"][0]["candidates"][0]["type"] == "host");

		// REQUIRE(parsed["media"][0].find("connection") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["connection"]["ip"] == "127.0.0.1");
		// REQUIRE(parsed["media"][0]["connection"]["version"] == 4);

		// REQUIRE(parsed["media"][0]["direction"] == "recvonly");
		// REQUIRE(parsed["media"][0]["endOfCandidates"] == "end-of-candidates");

		// REQUIRE(parsed["media"][0].find("fmtp") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["fmtp"].size() == 1);
		// REQUIRE(parsed["media"][0]["fmtp"][0]["config"] == "usedtx=1;useinbandfec=1");
		// REQUIRE(parsed["media"][0]["fmtp"][0]["payload"] == 96);

		// REQUIRE(parsed["media"][0]["iceOptions"] == "renomination");
		// REQUIRE(parsed["media"][0]["icePwd"] == "e46UjXntt0K/xTncQcDBQePn");
		// REQUIRE(parsed["media"][0]["iceUfrag"] == "5I2uVefP13X1wzOY");

		// REQUIRE(parsed["media"][0]["mid"] == "audio");
		// REQUIRE(parsed["media"][0]["payloads"] == "0 96");
		// REQUIRE(parsed["media"][0]["port"] == 7);
		// REQUIRE(parsed["media"][0]["protocol"] == "RTP/SAVPF");
		// REQUIRE(parsed["media"][0]["rtcpMux"] == "rtcp-mux");
		// REQUIRE(parsed["media"][0]["rtcpRsize"] == "rtcp-rsize");
		// REQUIRE(parsed["media"][0]["setup"] == "active");
		// REQUIRE(parsed["media"][0]["type"] == "audio");

		// REQUIRE(parsed["media"][0].find("rtp") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["rtp"].size() == 2);
		// REQUIRE(parsed["media"][0]["rtp"][0]["codec"] == "PCMU");
		// REQUIRE(parsed["media"][0]["rtp"][0]["payload"] == 0);
		// REQUIRE(parsed["media"][0]["rtp"][0]["rate"] == 8000);
		// REQUIRE(parsed["media"][0]["rtp"][1]["codec"] == "opus");
		// REQUIRE(parsed["media"][0]["rtp"][1]["payload"] == 96);
		// REQUIRE(parsed["media"][0]["rtp"][1]["rate"] == 48000);

		REQUIRE(parsed.find("msidSemantic") != parsed.end());
		REQUIRE(parsed["msidSemantic"]["semantic"] == "WMS");
		REQUIRE(parsed["msidSemantic"]["token"] == "*");

		REQUIRE(parsed["name"] == "-");

		REQUIRE(parsed.find("origin") != parsed.end());
		REQUIRE(parsed["origin"]["address"] == "0.0.0.0");
		REQUIRE(parsed["origin"]["ipVer"] == 4);
		REQUIRE(parsed["origin"]["netType"] == "IN");
		REQUIRE(parsed["origin"]["sessionVersion"] == 1);
		REQUIRE(parsed["origin"]["username"] == "libmediasoupclient");

		REQUIRE(parsed.find("timing") != parsed.end());
		REQUIRE(parsed["timing"]["start"] == 0);
		REQUIRE(parsed["timing"]["stop"] == 0);

		REQUIRE(parsed["version"] == 0);

		delete remoteSdp;
	}

	SECTION("audio and video", "[CreateAnswerSdp]")
	{
		auto dtlsParameters = R"({
			"role"         : "client",
			"fingerprints" :
			[
				{
					"algorithm" : "sha-256",
					"value"     : "79:14:AB:AB:93:7F:07:E8:91:1A:11:16:36:D0:11:66:C4:4F:31:A0:74:46:65:58:70:E5:09:95:48:F4:4B:D9"
				}
			]
		})"_json;

		auto iceParameters = R"({
			"usernameFragment" : "5I2uVefP13X1wzOY",
			"password"         : "e46UjXntt0K/xTncQcDBQePn"
		})"_json;

		auto iceCandidates = R"([
			{
				"foundation" : "1162875081",
				"component"  : 1,
				"protocol"   : "udp",
				"priority"   : 2113937151,
				"ip"         : "192.168.34.75",
				"port"       : 60017,
				"type"       : "host",
				"generation" : 0
			}
		])"_json;

		auto audioCodecs = R"([
			{
				"mimeType"             : "audio/PCMU",
				"kind"                 : "audio",
				"clockRate"            : 8000,
				"payloadType"          : 0,
				"rtcpFeedback"         : [],
				"parameters"           : {}
			},
			{
				"mimeType"             : "audio/opus",
				"kind"                 : "audio",
				"clockRate"            : 48000,
				"payloadType"          : 96,
				"rtcpFeedback"         : [],
				"parameters"           :
				{
					"usedtx"       : "1",
					"useinbandfec" : "1"
				}
			}
		])"_json;

		auto headerExtensions = R"([
			{
				"value" : 1,
				"uri"   : "URI-toffset"
			}
		])"_json;

		/* clang-format off */
		json sendingRtpParametersByKind =
		{
			{
				"audio",
				{
					{ "codecs",           audioCodecs      },
					{ "headerExtensions", headerExtensions }
				}
			}
		};

		json transportRemoteParameters =
		{
			{ "iceParameters", iceParameters   },
			{ "iceCandidates", iceCandidates   },
			{ "dtlsParameters", dtlsParameters }
		};
		/* clang-format on */

		auto* remoteSdp = new Sdp::RemoteSdp(iceParameters, iceCandidates, dtlsParameters);

		auto sdp         = helpers::readFile("test/sdp/data/audio_video.sdp");
		auto localSdpObj = sdptransform::parse(sdp);

		auto sdpAnswer = remoteSdp->GetSdp();
		auto parsed    = sdptransform::parse(sdpAnswer);

		REQUIRE(parsed.find("fingerprint") != parsed.end());
		REQUIRE(
		  parsed["fingerprint"]["hash"] ==
		  "79:14:AB:AB:93:7F:07:E8:91:1A:11:16:36:D0:11:66:C4:4F:31:A0:74:46:65:58:70:E5:09:95:48:F4:4B:D9");
		REQUIRE(parsed["fingerprint"]["type"] == "sha-256");

		REQUIRE(parsed.find("groups") != parsed.end());
		REQUIRE(parsed["groups"].size() == 1);
		// REQUIRE(parsed["groups"][0]["mids"] == "audio video");
		REQUIRE(parsed["groups"][0]["type"] == "BUNDLE");

		// REQUIRE(parsed.find("media") != parsed.end());
		// REQUIRE(parsed["media"].size() == 2);

		// REQUIRE(parsed["media"][0].find("candidates") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["candidates"].size() == 1);
		// REQUIRE(parsed["media"][0]["candidates"][0]["component"] == 1);
		// REQUIRE(parsed["media"][0]["candidates"][0]["foundation"] == "1162875081");
		// REQUIRE(parsed["media"][0]["candidates"][0]["ip"] == "192.168.34.75");
		// REQUIRE(parsed["media"][0]["candidates"][0]["port"] == 60017);
		// REQUIRE(parsed["media"][0]["candidates"][0]["priority"] == 2113937151);
		// REQUIRE(parsed["media"][0]["candidates"][0]["transport"] == "udp");
		// REQUIRE(parsed["media"][0]["candidates"][0]["type"] == "host");

		// REQUIRE(parsed["media"][0].find("connection") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["connection"]["ip"] == "127.0.0.1");
		// REQUIRE(parsed["media"][0]["connection"]["version"] == 4);

		// REQUIRE(parsed["media"][0]["direction"] == "recvonly");
		// REQUIRE(parsed["media"][0]["endOfCandidates"] == "end-of-candidates");

		// REQUIRE(parsed["media"][0].find("fmtp") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["fmtp"].size() == 1);
		// REQUIRE(parsed["media"][0]["fmtp"][0]["config"] == "usedtx=1;useinbandfec=1");
		// REQUIRE(parsed["media"][0]["fmtp"][0]["payload"] == 96);

		// REQUIRE(parsed["media"][0]["iceOptions"] == "renomination");
		// REQUIRE(parsed["media"][0]["icePwd"] == "e46UjXntt0K/xTncQcDBQePn");
		// REQUIRE(parsed["media"][0]["iceUfrag"] == "5I2uVefP13X1wzOY");

		// REQUIRE(parsed["media"][0]["mid"] == "audio");
		// REQUIRE(parsed["media"][0]["payloads"] == "0 96");
		// REQUIRE(parsed["media"][0]["port"] == 7);
		// REQUIRE(parsed["media"][0]["protocol"] == "RTP/SAVPF");
		// REQUIRE(parsed["media"][0]["rtcpMux"] == "rtcp-mux");
		// REQUIRE(parsed["media"][0]["rtcpRsize"] == "rtcp-rsize");
		// REQUIRE(parsed["media"][0]["setup"] == "active");
		// REQUIRE(parsed["media"][0]["type"] == "audio");

		// REQUIRE(parsed["media"][0].find("rtp") != parsed["media"][0].end());
		// REQUIRE(parsed["media"][0]["rtp"].size() == 2);
		// REQUIRE(parsed["media"][0]["rtp"][0]["codec"] == "PCMU");
		// REQUIRE(parsed["media"][0]["rtp"][0]["payload"] == 0);
		// REQUIRE(parsed["media"][0]["rtp"][0]["rate"] == 8000);
		// REQUIRE(parsed["media"][0]["rtp"][1]["codec"] == "opus");
		// REQUIRE(parsed["media"][0]["rtp"][1]["payload"] == 96);
		// REQUIRE(parsed["media"][0]["rtp"][1]["rate"] == 48000);

		REQUIRE(parsed.find("msidSemantic") != parsed.end());
		REQUIRE(parsed["msidSemantic"]["semantic"] == "WMS");
		REQUIRE(parsed["msidSemantic"]["token"] == "*");

		REQUIRE(parsed["name"] == "-");

		REQUIRE(parsed.find("origin") != parsed.end());
		REQUIRE(parsed["origin"]["address"] == "0.0.0.0");
		REQUIRE(parsed["origin"]["ipVer"] == 4);
		REQUIRE(parsed["origin"]["netType"] == "IN");
		REQUIRE(parsed["origin"]["sessionVersion"] == 1);
		REQUIRE(parsed["origin"]["username"] == "libmediasoupclient");

		REQUIRE(parsed.find("timing") != parsed.end());
		REQUIRE(parsed["timing"]["start"] == 0);
		REQUIRE(parsed["timing"]["stop"] == 0);

		REQUIRE(parsed["version"] == 0);

		delete remoteSdp;
	}
}
