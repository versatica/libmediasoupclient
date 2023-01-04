#include "sdptransform.hpp"
#include "helpers.hpp"
#include "catch_amalgamated.hpp"

SCENARIO("normalSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/normal.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");

	REQUIRE(session.at("origin").at("username") == "-");
	REQUIRE(session.at("origin").at("sessionId") == 20518);
	REQUIRE(session.at("origin").at("sessionVersion") == 0);
	REQUIRE(session.at("origin").at("netType") == "IN");
	REQUIRE(session.at("origin").at("ipVer") == 4);
	REQUIRE(session.at("origin").at("address") == "203.0.113.1");

	// Testing json efficient access.
	auto originIterator = session.find("origin");
	auto addressIterator = originIterator->find("address");
	auto sessionIdIterator = originIterator->find("sessionId");

	REQUIRE(addressIterator != originIterator->end());
	REQUIRE(*addressIterator == "203.0.113.1");
	REQUIRE(addressIterator->get<std::string>() == "203.0.113.1");
	REQUIRE(sessionIdIterator != originIterator->end());
	REQUIRE(*sessionIdIterator == 20518);
	REQUIRE(sessionIdIterator->get<int>() == 20518);
	REQUIRE(sessionIdIterator->get<uint16_t>() == 20518);
	REQUIRE(sessionIdIterator->get<int32_t>() == 20518);

	REQUIRE(session.at("connection").at("ip") == "203.0.113.1");
	REQUIRE(session.at("connection").at("version") == 4);

	// Global ICE and fingerprint.
	REQUIRE(session.at("iceUfrag") == "F7gI");
	REQUIRE(session.at("icePwd") == "x9cml/YzichV2+XlhiMu8g");

	auto& audio = media[0];
	auto audioPayloads = sdptransform::parsePayloads(audio.at("payloads"));

	REQUIRE(audioPayloads == R"([ 0, 96 ])"_json);

	REQUIRE(audio.at("type") == "audio");
	REQUIRE(audio.at("port") == 54400);
	REQUIRE(audio.at("protocol") == "RTP/SAVPF");
	REQUIRE(audio.at("direction") == "sendrecv");
	REQUIRE(audio.at("rtp")[0].at("payload") == 0);
	REQUIRE(audio.at("rtp")[0].at("codec") == "PCMU");
	REQUIRE(audio.at("rtp")[0].at("rate") == 8000);
	REQUIRE(audio.at("rtp")[1].at("payload") == 96);
	REQUIRE(audio.at("rtp")[1].at("codec") == "opus");
	REQUIRE(audio.at("rtp")[1].at("rate") == 48000);
	REQUIRE(
		audio.at("ext")[0] ==
		R"({
			"value" : 1,
			"uri"   : "URI-toffset"
		})"_json
	);
	REQUIRE(
		audio.at("ext")[1] ==
		R"({
			"value"     : 2,
			"direction" : "recvonly",
			"uri"       : "URI-gps-string"
		})"_json
	);
	REQUIRE(audio.at("extmapAllowMixed") == "extmap-allow-mixed");

	auto& video = media[1];
	auto videoPayloads = sdptransform::parsePayloads(video.at("payloads"));

	REQUIRE(videoPayloads == R"([ 97, 98 ])"_json);

	REQUIRE(video.at("type") == "video");
	REQUIRE(video.at("port") == 55400);
	REQUIRE(video.at("protocol") == "RTP/SAVPF");
	REQUIRE(video.at("direction") == "sendrecv");
	REQUIRE(video.at("rtp")[0].at("payload") == 97);
	REQUIRE(video.at("rtp")[0].at("codec") == "H264");
	REQUIRE(video.at("rtp")[0].at("rate") == 90000);
	REQUIRE(video.at("fmtp")[0].at("payload") == 97);

	auto vidFmtp = sdptransform::parseParams(video.at("fmtp")[0].at("config"));

	REQUIRE(vidFmtp.at("profile-level-id") == "42e034");
	REQUIRE(vidFmtp.at("packetization-mode") == 1);
	REQUIRE(vidFmtp.at("sprop-parameter-sets") == "Z0IAH5WoFAFuQA==,aM48gA==");

	// Testing json efficient access.
	auto profileLevelIdIterator = vidFmtp.find("profile-level-id");

	REQUIRE(profileLevelIdIterator != vidFmtp.end());
	REQUIRE(*profileLevelIdIterator == "42e034");
	REQUIRE(profileLevelIdIterator->get<std::string>() == "42e034");

	REQUIRE(video.at("fmtp")[1].at("payload") == 98);

	auto vidFmtp2 = sdptransform::parseParams(video.at("fmtp")[1].at("config"));

	REQUIRE(vidFmtp2.at("minptime") == 10);
	REQUIRE(vidFmtp2.at("useinbandfec") == 1);

	REQUIRE(video.at("rtp")[1].at("payload") == 98);
	REQUIRE(video.at("rtp")[1].at("codec") == "VP8");
	REQUIRE(video.at("rtp")[1].at("rate") == 90000);
	REQUIRE(video.at("rtcpFb")[0].at("payload") == "*");
	REQUIRE(video.at("rtcpFb")[0].at("type") == "nack");
	REQUIRE(video.at("rtcpFb")[1].at("payload") == "98");
	REQUIRE(video.at("rtcpFb")[1].at("type") == "nack");
	REQUIRE(video.at("rtcpFb")[1].at("subtype") == "rpsi");
	REQUIRE(video.at("rtcpFbTrrInt")[0].at("payload") == "98");
	REQUIRE(video.at("rtcpFbTrrInt")[0].at("value") == 100);
	REQUIRE(video.at("crypto")[0].at("id") == 1);
	REQUIRE(video.at("crypto")[0].at("suite") == "AES_CM_128_HMAC_SHA1_32");
	REQUIRE(video.at("crypto")[0].at("config") == "inline:keNcG3HezSNID7LmfDa9J4lfdUL8W1F7TNJKcbuy|2^20|1:32");
	REQUIRE(video.at("ssrcs").size() == 3);
	REQUIRE(
		video.at("ssrcs")[0] ==
		R"({
			"id"        : 1399694169,
			"attribute" : "foo",
			"value"     : "bar"
		})"_json
	);
	REQUIRE(
		video.at("ssrcs")[1] ==
		R"({
			"id"        : 1399694169,
			"attribute" : "baz"
		})"_json
	);
	REQUIRE(
		video.at("ssrcs")[2] ==
		R"({
			"id"        : 1399694169,
			"attribute" : "foo-bar",
			"value"     : "baz"
		})"_json
	);

	auto& cs = audio.at("candidates");

	REQUIRE(cs.size() == 4);
	REQUIRE(cs[0].at("foundation") == "0");
	REQUIRE(cs[0].at("component") == 1);
	REQUIRE(cs[0].at("transport") == "UDP");
	REQUIRE(cs[0].at("priority") == 2113667327);
	REQUIRE(cs[0].at("ip") == "203.0.113.1");
	REQUIRE(cs[0].at("port") == 54400);
	REQUIRE(cs[0].at("type") == "host");
	REQUIRE(cs[1].at("foundation") == "1");
	REQUIRE(cs[1].at("component") == 2);
	REQUIRE(cs[1].at("transport") == "UDP");
	REQUIRE(cs[1].at("priority") == 2113667326);
	REQUIRE(cs[1].at("ip") == "203.0.113.1");
	REQUIRE(cs[1].at("port") == 54401);
	REQUIRE(cs[1].at("type") == "host");
	REQUIRE(cs[2].at("foundation") == "2");
	REQUIRE(cs[2].at("component") == 1);
	REQUIRE(cs[2].at("transport") == "UDP");
	REQUIRE(cs[2].at("priority") == 1686052607);
	REQUIRE(cs[2].at("ip") == "203.0.113.1");
	REQUIRE(cs[2].at("port") == 54402);
	REQUIRE(cs[2].at("type") == "srflx");
	REQUIRE(cs[2].at("raddr") == "192.168.1.145");
	REQUIRE(cs[2].at("rport") == 54402);
	REQUIRE(cs[2].at("generation") == 0);
	REQUIRE(cs[2].at("network-id") == 3);
	REQUIRE(cs[2].at("network-cost") == 10);
	REQUIRE(cs[3].at("foundation") == "3");
	REQUIRE(cs[3].at("component") == 2);
	REQUIRE(cs[3].at("transport") == "UDP");
	REQUIRE(cs[3].at("priority") == 1686052606);
	REQUIRE(cs[3].at("ip") == "203.0.113.1");
	REQUIRE(cs[3].at("port") == 54403);
	REQUIRE(cs[3].at("type") == "srflx");
	REQUIRE(cs[3].at("raddr") == "192.168.1.145");
	REQUIRE(cs[3].at("rport") == 54403);
	REQUIRE(cs[3].at("generation") == 0);
	REQUIRE(cs[3].at("network-id") == 3);
	REQUIRE(cs[3].at("network-cost") == 10);

	auto& cs2 = video.at("candidates");

	REQUIRE(cs2[2].find("network-cost") == cs2[2].end());
	REQUIRE(cs2[3].find("network-cost") == cs2[3].end());

	REQUIRE(media.size() == 2);

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("hackySdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/hacky.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");

	REQUIRE(session.at("origin").at("sessionId") == 3710604898417546434);
	REQUIRE(session.at("groups").size() == 1);
	REQUIRE(session.at("groups")[0].at("type") == "BUNDLE");
	REQUIRE(session.at("groups")[0].at("mids") == "audio video");
	REQUIRE(session.at("msidSemantic").at("semantic") == "WMS");
	REQUIRE(session.at("msidSemantic").at("token") == "Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlV");

	// Verify a=rtcp:65179 IN IP4 193.84.77.194.
	REQUIRE(media[0].at("rtcp").at("port") == 1);
	REQUIRE(media[0].at("rtcp").at("netType") == "IN");
	REQUIRE(media[0].at("rtcp").at("ipVer") == 4);
	REQUIRE(media[0].at("rtcp").at("address") == "0.0.0.0");

	// Verify ICE TCP types.
	REQUIRE(
		media[0].at("candidates")[0].find("tcptype") == media[0].at("candidates")[0].end()
	);
	REQUIRE(media[0].at("candidates")[1].at("tcptype") == "active");
	REQUIRE(media[0].at("candidates")[1].at("transport") == "tcp");
	REQUIRE(media[0].at("candidates")[1].at("generation") == 0);
	REQUIRE(media[0].at("candidates")[1].at("type") == "host");
	REQUIRE(
		media[0].at("candidates")[2].find("generation") == media[0].at("candidates")[2].end()
	);
	REQUIRE(media[0].at("candidates")[2].at("type") == "host");
	REQUIRE(media[0].at("candidates")[2].at("tcptype") == "active");
	REQUIRE(media[0].at("candidates")[3].at("tcptype") == "passive");
	REQUIRE(media[0].at("candidates")[4].at("tcptype") == "so");
	// raddr + rport + tcptype + generation.
	REQUIRE(media[0].at("candidates")[5].at("type") == "srflx");
	REQUIRE(media[0].at("candidates")[5].at("rport") == 9);
	REQUIRE(media[0].at("candidates")[5].at("raddr") == "10.0.1.1");
	REQUIRE(media[0].at("candidates")[5].at("tcptype") == "active");
	REQUIRE(media[0].at("candidates")[6].at("tcptype") == "passive");
	REQUIRE(media[0].at("candidates")[6].at("rport") == 8998);
	REQUIRE(media[0].at("candidates")[6].at("raddr") == "10.0.1.1");
	REQUIRE(media[0].at("candidates")[6].at("generation") == 5);

	// And verify it works without specifying the IP.
	REQUIRE(media[1].at("rtcp").at("port") == 12312);
	REQUIRE(media[1].at("rtcp").find("netType") == media[1].at("rtcp").end());
	REQUIRE(media[1].at("rtcp").find("ipVer") == media[1].at("rtcp").end());
	REQUIRE(media[1].at("rtcp").find("address") == media[1].at("rtcp").end());

	// Verify a=rtpmap:126 telephone-event/8000.
	auto lastRtp = media[0].at("rtp").size() - 1;

	REQUIRE(media[0].at("rtp")[lastRtp].at("codec") == "telephone-event");
	REQUIRE(media[0].at("rtp")[lastRtp].at("rate") == 8000);

	REQUIRE(media[0].at("iceOptions") == "google-ice");
	REQUIRE(media[0].at("maxptime") == 60);
	REQUIRE(media[0].at("rtcpMux") == "rtcp-mux");

	REQUIRE(media[0].at("rtp")[0].at("codec") == "opus");
	REQUIRE(media[0].at("rtp")[0].at("encoding") == "2");

	REQUIRE(media[0].at("ssrcs").size() == 4);

	auto& ssrcs = media[0].at("ssrcs");

	REQUIRE(
		ssrcs[0] ==
		R"({
			"id"        : 2754920552,
			"attribute" : "cname",
			"value"     : "t9YU8M1UxTF8Y1A1"
		})"_json
	);
	REQUIRE(
		ssrcs[1] ==
		R"({
			"id"        : 2754920552,
			"attribute" : "msid",
			"value"     : "Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlV Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlVa0"
		})"_json
	);
	REQUIRE(
		ssrcs[2] ==
		R"({
			"id"        : 2754920552,
			"attribute" : "mslabel",
			"value"     : "Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlV"
		})"_json
	);
	REQUIRE(
		ssrcs[3] ==
		R"({
			"id"        : 2754920552,
			"attribute" : "label",
			"value"     : "Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlVa0"
		})"_json
	);

	// Verify a=sctpmap:5000 webrtc-datachannel 1024.
	REQUIRE(media[2].find("sctpmap") != media[2].end());
	REQUIRE(media[2].at("sctpmap").at("sctpmapNumber") == 5000);
	REQUIRE(media[2].at("sctpmap").at("app") == "webrtc-datachannel");
	REQUIRE(media[2].at("sctpmap").at("maxMessageSize") == 1024);

	// Verify a=framerate:29.97.
	REQUIRE(media[1].at("framerate") == 1234);
	REQUIRE(media[2].at("framerate") == double{ 29.97 });

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("iceliteSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/icelite.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.at("icelite") == "ice-lite");

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("invalidSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/invalid.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");

	// Verify a=rtcp:65179 IN IP4 193.84.77.194-
	REQUIRE(media[0].at("rtcp").at("port") == 1);
	REQUIRE(media[0].at("rtcp").at("netType") == "IN");
	REQUIRE(media[0].at("rtcp").at("ipVer") == 7);
	REQUIRE(media[0].at("rtcp").at("address") == "X");
	REQUIRE(media[0].at("invalid").size() == 1); // f= was lost.
	REQUIRE(media[0].at("invalid")[0].at("value") == "goo:hithere");

	auto newSdp = sdptransform::write(session);

	// Append the wrong (so lost) f= line.
	newSdp += "f=invalid:yes\r\n";

	REQUIRE(newSdp == sdp);
}

SCENARIO("jssipSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/jssip.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& audio = media[0];
	auto& audCands = audio.at("candidates");

	REQUIRE(audCands.size() == 6);

	// Testing ice optionals.
	REQUIRE(
		audCands[0] ==
		R"({
			"foundation" : "1162875081",
			"component"  : 1,
			"transport"  : "udp",
			"priority"   : 2113937151,
			"ip"         : "192.168.34.75",
			"port"       : 60017,
			"type"       : "host",
			"generation" : 0
		})"_json
	);
	REQUIRE(
		audCands[2] ==
		R"({
			"foundation" : "3289912957",
			"component"  : 1,
			"transport"  : "udp",
			"priority"   : 1845501695,
			"ip"         : "193.84.77.194",
			"port"       : 60017,
			"type"       : "srflx",
			"raddr"      : "192.168.34.75",
			"rport"      : 60017,
			"generation" : 0
		})"_json
	);
	REQUIRE(
		audCands[4] ==
		R"({
			"foundation" : "198437945",
			"component"  : 1,
			"transport"  : "tcp",
			"priority"   : 1509957375,
			"ip"         : "192.168.34.75",
			"port"       : 0,
			"type"       : "host",
			"generation" : 0
		})"_json
	);
}

SCENARIO("jsepSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/jsep.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");

	REQUIRE(media.size() == 2);

	auto& video = media[1];

	REQUIRE(video.at("ssrcGroups").size() == 1);
	REQUIRE(
		video.at("ssrcGroups")[0] ==
		R"({
			"semantics" : "FID",
			"ssrcs"     : "1366781083 1366781084"
		})"_json
	);

	REQUIRE(
		video.at("msid") ==
		"61317484-2ed4-49d7-9eb7-1414322a7aae f30bdb4a-5db8-49b5-bcdc-e0c9a23172e0"
	);

	REQUIRE(video.find("rtcpRsize") != video.end());
	REQUIRE(video.find("endOfCandidates") != video.end());
}

SCENARIO("alacSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/alac.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& audio = media[0];
	auto audioPayloads = sdptransform::parsePayloads(audio.at("payloads"));

	REQUIRE(audioPayloads == R"([ 96 ])"_json);

	REQUIRE(audio.at("type") == "audio");
	REQUIRE(audio.at("protocol") == "RTP/AVP");
	REQUIRE(audio.at("fmtp")[0].at("payload") == 96);
	REQUIRE(audio.at("fmtp")[0].at("config") == "352 0 16 40 10 14 2 255 0 0 44100");
	REQUIRE(audio.at("rtp")[0].at("payload") == 96);
	REQUIRE(audio.at("rtp")[0].at("codec") == "AppleLossless");
	REQUIRE(audio.at("rtp")[0].find("rate") == audio.at("rtp")[0].end());
	REQUIRE(audio.at("rtp")[0].find("encoding") == audio.at("rtp")[0].end());

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("onvifSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/onvif.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& audio = media[0];

	REQUIRE(audio.at("type") == "audio");
	REQUIRE(audio.at("port") == 0);
	REQUIRE(audio.at("protocol") == "RTP/AVP");
	REQUIRE(audio.at("control") == "rtsp://example.com/onvif_camera/audio");
	REQUIRE(audio.at("payloads") == "0");

	auto& video = media[1];

	REQUIRE(video.at("type") == "video");
	REQUIRE(video.at("port") == 0);
	REQUIRE(video.at("protocol") == "RTP/AVP");
	REQUIRE(video.at("control") == "rtsp://example.com/onvif_camera/video");
	REQUIRE(video.at("payloads") == "26");

	auto& application = media[2];

	REQUIRE(application.at("type") == "application");
	REQUIRE(application.at("port") == 0);
	REQUIRE(application.at("protocol") == "RTP/AVP");
	REQUIRE(application.at("control") == "rtsp://example.com/onvif_camera/metadata");
	REQUIRE(application.at("payloads") == "107");
	REQUIRE(application.at("direction") == "recvonly");
	REQUIRE(application.at("rtp")[0].at("payload") == 107);
	REQUIRE(application.at("rtp")[0].at("codec") == "vnd.onvif.metadata");
	REQUIRE(application.at("rtp")[0].at("rate") == 90000);
	REQUIRE(application.at("rtp")[0].find("encoding") == application.at("rtp")[0].end());

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("ssrcSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/ssrc.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& video = media[1];

	REQUIRE(video.at("ssrcGroups").size() == 2);
	REQUIRE(
		video.at("ssrcGroups") ==
		R"([
			{
				"semantics" : "FID",
				"ssrcs"     : "3004364195 1126032854"
			},
			{
				"semantics" : "FEC-FR",
				"ssrcs"     : "3004364195 1080772241"
			}
		])"_json
	);
}

SCENARIO("simulcastSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/simulcast.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& video = media[1];

	REQUIRE(video.at("type") == "video");

	// Test rid 1.
	REQUIRE(
		video.at("rids")[0] ==
		R"({
			"id"        : "1",
			"direction" : "send",
			"params"    : "pt=97;max-width=1280;max-height=720;max-fps=30"
		})"_json
	);

	// Test rid 2.
	REQUIRE(
		video.at("rids")[1] ==
		R"({
			"id"        : "2",
			"direction" : "send",
			"params"    : "pt=98"
		})"_json
	);

	// Test rid 3.
	REQUIRE(
		video.at("rids")[2] ==
		R"({
			"id"        : "3",
			"direction" : "send",
			"params"    : "pt=99"
		})"_json
	);

	// Test rid 4.
	REQUIRE(
		video.at("rids")[3] ==
		R"({
			"id"        : "4",
			"direction" : "send",
			"params"    : "pt=100"
		})"_json
	);

	// Test rid 5.
	REQUIRE(
		video.at("rids")[4] ==
		R"({
			"id"        : "c",
			"direction" : "recv",
			"params"    : "pt=97"
		})"_json
	);

	// // Test rid 1 params.
	auto rid1Params = sdptransform::parseParams(video.at("rids")[0].at("params"));

	REQUIRE(
		rid1Params ==
		R"({
			"pt"         : 97,
			"max-width"  : 1280,
			"max-height" : 720,
			"max-fps"    : 30
		})"_json
	);

	// Test rid 2 params.
	auto rid2Params = sdptransform::parseParams(video.at("rids")[1].at("params"));

	REQUIRE(
		rid2Params ==
		R"({
			"pt" : 98
		})"_json
	);

	// Test rid 3 params.
	auto rid3Params = sdptransform::parseParams(video.at("rids")[2].at("params"));

	REQUIRE(
		rid3Params ==
		R"({
			"pt" : 99
		})"_json
	);

	// Test rid 4 params.
	auto rid4Params = sdptransform::parseParams(video.at("rids")[3].at("params"));

	REQUIRE(
		rid4Params ==
		R"({
			"pt" : 100
		})"_json
	);

	// Test rid 5 params.
	auto rid5Params = sdptransform::parseParams(video.at("rids")[4].at("params"));

	REQUIRE(
		rid5Params ==
		R"({
			"pt" : 97
		})"_json
	);

	// Test imageattr lines.
	REQUIRE(video.at("imageattrs").size() == 5);

	// Test imageattr 1.
	REQUIRE(
		video.at("imageattrs")[0] ==
		R"({
			"pt"     : "97",
			"dir1"   : "send",
			"attrs1" : "[x=1280,y=720]",
			"dir2"   : "recv",
			"attrs2" : "[x=1280,y=720] [x=320,y=180] [x=160,y=90]"
		})"_json
	);

	// Test imageattr 2.
	REQUIRE(
		video.at("imageattrs")[1] ==
		R"({
			"pt"     : "98",
			"dir1"   : "send",
			"attrs1" : "[x=320,y=180,sar=1.1,q=0.6]"
		})"_json
	);

	// Test imageattr 3.
	REQUIRE(
		video.at("imageattrs")[2] ==
		R"({
			"pt"     : "99",
			"dir1"   : "send",
			"attrs1" : "[x=160,y=90]"
		})"_json
	);

	// Test imageattr 4.
	REQUIRE(
		video.at("imageattrs")[3] ==
		R"({
			"pt"     : "100",
			"dir1"   : "recv",
			"attrs1" : "[x=1280,y=720] [x=320,y=180]",
			"dir2"   : "send",
			"attrs2" : "[x=1280,y=720]"
		})"_json
	);

	// Test imageattr 5.
	REQUIRE(
		video.at("imageattrs")[4] ==
		R"({
			"pt"     : "*",
			"dir1"   : "recv",
			"attrs1" : "*"
		})"_json
	);

	// Test imageattr 2 send params.
	auto imageattr2SendParams =
		sdptransform::parseImageAttributes(video.at("imageattrs")[1].at("attrs1"));

	REQUIRE(
		imageattr2SendParams ==
		R"([
			{
				"x"   : 320,
				"y"   : 180,
				"sar" : 1.1,
				"q"   : 0.6
			}
		])"_json
	);

	// Test imageattr 3 send params.
	auto imageattr3SendParams =
		sdptransform::parseImageAttributes(video.at("imageattrs")[2].at("attrs1"));

	REQUIRE(
		imageattr3SendParams ==
		R"([
			{
				"x" : 160,
				"y" : 90
			}
		])"_json
	);

	// Test imageattr 4 recv params.
	auto imageattr4RecvParams =
		sdptransform::parseImageAttributes(video.at("imageattrs")[3].at("attrs1"));

	REQUIRE(
		imageattr4RecvParams ==
		R"([
			{
				"x" : 1280,
				"y" : 720
			},
			{
				"x" : 320,
				"y" : 180
			}
		])"_json
	);

	// Test imageattr 4 send params.
	auto imageattr4SendParams =
		sdptransform::parseImageAttributes(video.at("imageattrs")[3].at("attrs2"));

	REQUIRE(
		imageattr4SendParams ==
		R"([
			{
				"x" : 1280,
				"y" : 720
			}
		])"_json
	);

	// Test imageattr 5 recv params.
	auto imageattr5RecvParams =
		sdptransform::parseImageAttributes(video.at("imageattrs")[4].at("attrs1"));

	REQUIRE(imageattr5RecvParams == "*");

	// Test simulcast line.
	REQUIRE(
		video.at("simulcast") ==
		R"({
			"dir1"  : "send",
			"list1" : "1,~4;2;3",
			"dir2"  : "recv",
			"list2" : "c"
		})"_json
	);

	// Test simulcast send streams.
	auto simulcastSendStreams =
		sdptransform::parseSimulcastStreamList(video.at("simulcast").at("list1"));

	REQUIRE(
		simulcastSendStreams ==
		R"([
			[ { "scid": "1", "paused": false }, { "scid": "4", "paused": true } ],
			[ { "scid": "2", "paused": false } ],
			[ { "scid": "3", "paused": false } ]
		])"_json
	);

	// Test simulcast recv streams.
	auto simulcastRecvStreams =
		sdptransform::parseSimulcastStreamList(video.at("simulcast").at("list2"));

	REQUIRE(
		simulcastRecvStreams ==
		R"([
			[ { "scid": "c", "paused": false } ]
		])"_json
	);

	// Test simulcast version 03 line.
	REQUIRE(
		video.at("simulcast_03") ==
		R"({
			"value" : "send rid=1,4;2;3 paused=4 recv rid=c"
		})"_json
	);

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("st2022-6Sdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/st2022-6.sdp");
	auto session = sdptransform::parse(sdp);

	// Session sanity check.
	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());
	auto& media = session.at("media");

	// No invalid node
	REQUIRE(media.find("invalid") == media.end());

	// Check sourceFilter node exists.
	auto& video = media[0];
	REQUIRE(video.find("sourceFilter") != video.end());
	auto& sourceFilter = video.at("sourceFilter");

	// Check expected values are present.
	REQUIRE(sourceFilter.at("filterMode") == "incl");
	REQUIRE(sourceFilter.at("netType") == "IN");
	REQUIRE(sourceFilter.at("addressTypes") == "IP4");
	REQUIRE(sourceFilter.at("destAddress") == "239.0.0.1");
	REQUIRE(sourceFilter.at("srcList") == "192.168.20.20");
}

SCENARIO("st2110-20Sdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/st2110-20.sdp");
	auto session = sdptransform::parse(sdp);

	// Session sanity check.
	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());
	auto& media = session.at("media");

	// No invalid node
	REQUIRE(media.find("invalid") == media.end());

	// Check sourceFilter node exists.
	auto& video = media[0];
	REQUIRE(video.find("sourceFilter") != video.end());
	auto& sourceFilter = video.at("sourceFilter");

	// Check expected values are present.
	REQUIRE(sourceFilter.at("filterMode") == "incl");
	REQUIRE(sourceFilter.at("netType") == "IN");
	REQUIRE(sourceFilter.at("addressTypes") == "IP4");
	REQUIRE(sourceFilter.at("destAddress") == "239.100.9.10");
	REQUIRE(sourceFilter.at("srcList") == "192.168.100.2");

	auto fmtp0Params = sdptransform::parseParams(video.at("fmtp")[0].at("config"));

	REQUIRE(
		fmtp0Params ==
		R"({
			"sampling"       : "YCbCr-4:2:2",
			"width"          : 1280,
			"height"         : 720,
			"interlace"      : "",
			"exactframerate" : "60000/1001",
			"depth"          : 10,
			"TCS"            : "SDR",
			"colorimetry"    : "BT709",
			"PM"             : "2110GPM",
			"SSN"            : "ST2110-20:2017"
		})"_json
	);
}

SCENARIO("aes67", "[parse]")
{
	auto sdp = helpers::readFile("test/data/aes67.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& audio = media[0];
	auto audioPayloads = sdptransform::parsePayloads(audio.at("payloads"));

	REQUIRE(audioPayloads == R"([ 96 ])"_json);

	REQUIRE(audio.at("type") == "audio");
	REQUIRE(audio.at("port") == 5004);
	REQUIRE(audio.at("numPorts") == 2);
	REQUIRE(audio.at("protocol") == "RTP/AVP");
	REQUIRE(audio.at("rtp")[0].at("payload") == 96);
	REQUIRE(audio.at("rtp")[0].at("codec") == "L24");
	REQUIRE(audio.at("rtp")[0].at("rate") == 48000);
	REQUIRE(audio.at("rtp")[0].at("encoding") == "2");
	REQUIRE(audio.at("tsRefclk") == "ptp=IEEE1588-2008:00-1D-C1-FF-FE-12-00-A4:0");
	REQUIRE(audio.at("mediaclk") == "direct=0");

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("multicastttlSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/multicastttl.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");

	REQUIRE(session.at("origin").at("sessionId") == 1558439701980808);
	REQUIRE(session.at("origin").at("sessionVersion") == 1);
	REQUIRE(session.at("origin").at("netType") == "IN");
	REQUIRE(session.at("origin").at("ipVer") == 4);
	REQUIRE(session.at("origin").at("address") == "192.168.1.189");

	REQUIRE(session.at("connection").at("ip") == "224.2.36.42");
	REQUIRE(session.at("connection").at("version") == 4);
	REQUIRE(session.at("connection").at("ttl") == 15);

	auto& video = media[0];
	auto videoPayloads = sdptransform::parsePayloads(video.at("payloads"));

	REQUIRE(video.at("type") == "video");
	REQUIRE(video.at("port") == 6970);
	REQUIRE(video.at("protocol") == "RTP/AVP");
	REQUIRE(video.at("rtp")[0].at("payload") == 96);
	REQUIRE(video.at("rtp")[0].at("codec") == "H264");
	REQUIRE(video.at("rtp")[0].at("rate") == 90000);
	REQUIRE(video.at("fmtp")[0].at("payload") == 96);

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}

SCENARIO("extmapEncryptSdp", "[parse]")
{
	auto sdp = helpers::readFile("test/data/extmap-encrypt.sdp");
	auto session = sdptransform::parse(sdp);

	REQUIRE(session.size() > 0);
	REQUIRE(session.find("media") != session.end());

	auto& media = session.at("media");
	auto& audio = media[0];
	auto audioPayloads = sdptransform::parsePayloads(audio.at("payloads"));

	REQUIRE(audioPayloads == R"([ 96 ])"_json);

	REQUIRE(audio.at("type") == "audio");
	REQUIRE(audio.at("port") == 54400);
	REQUIRE(audio.at("protocol") == "RTP/SAVPF");
	REQUIRE(audio.at("rtp")[0].at("payload") == 96);
	REQUIRE(audio.at("rtp")[0].at("codec") == "opus");
	REQUIRE(audio.at("rtp")[0].at("rate") == 48000);
	REQUIRE(
		audio.at("ext")[0] ==
		R"({
			"value"     : 1,
			"direction" : "sendonly",
			"uri"       : "URI-toffset"
		})"_json
	);
	REQUIRE(
		audio.at("ext")[1] ==
		R"({
			"value" : 2,
			"uri"   : "urn:ietf:params:rtp-hdrext:toffset"
		})"_json
	);
	REQUIRE(
		audio.at("ext")[2] ==
		R"({
			"value"       : 3,
			"encrypt-uri" : "urn:ietf:params:rtp-hdrext:encrypt",
			"uri"         : "urn:ietf:params:rtp-hdrext:smpte-tc",
			"config"      : "25@600/24"
		})"_json
	);
	REQUIRE(
		audio.at("ext")[3] ==
		R"({
			"value"       : 4,
			"direction"   : "recvonly",
			"encrypt-uri" : "urn:ietf:params:rtp-hdrext:encrypt",
			"uri"         : "URI-gps-string"
		})"_json
	);

	REQUIRE(media.size() == 1);

	auto newSdp = sdptransform::write(session);

	REQUIRE(newSdp == sdp);
}
