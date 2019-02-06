#include "sdptransform.hpp"
#include <iostream>
#include <string>

static std::string sdpStr = R"(v=0
o=- 20518 0 IN IP4 203.0.113.1
s=
t=0 0
c=IN IP4 203.0.113.1
a=ice-ufrag:F7gI
a=ice-pwd:x9cml/YzichV2+XlhiMu8g
a=fingerprint:sha-1 42:89:c5:c6:55:9d:6e:c8:e8:83:55:2a:39:f9:b6:eb:e9:a3:a9:e7
m=audio 54400 RTP/SAVPF 0 96
a=rtpmap:0 PCMU/8000
a=rtpmap:96 opus/48000
a=ptime:20
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 54400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 54401 typ host
m=video 55400 RTP/SAVPF 97 98
a=rtcp-fb:* nack
a=rtpmap:97 H264/90000
a=fmtp:97 profile-level-id=4d0028;packetization-mode=1
a=rtcp-fb:97 trr-int 100
a=rtcp-fb:97 nack rpsi
a=rtpmap:98 VP8/90000
a=rtcp-fb:98 trr-int 100
a=rtcp-fb:98 nack rpsi
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 55400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 55401 typ host
a=ssrc:1399694169 foo:bar
a=ssrc:1399694169 baz
)";

static json session;

void printParserSection();
void printWriterSection();

int main(int argc, char* argv[])
{
	printParserSection();
	printWriterSection();

	return 0;
}

void printParserSection()
{
	std::cout << "### Parser" << std::endl << std::endl;

	std::cout << ">>> sdptransform::parse():" << std::endl << std::endl;

	session = sdptransform::parse(sdpStr);

	std::cout << session.dump(2) << std::endl << std::endl;

	std::cout << "sdptransform::parseParams():" << std::endl << std::endl;

	json params =
		sdptransform::parseParams(session.at("media")[1].at("fmtp")[0].at("config"));

	std::cout << params.dump(2) << std::endl << std::endl;

	std::cout << "sdptransform::parsePayloads():" << std::endl << std::endl;

	json payloads =
		sdptransform::parsePayloads(session.at("media")[1].at("payloads"));

	std::cout << payloads.dump(2) << std::endl << std::endl;

	std::cout << "sdptransform::parseImageAttributes():" << std::endl << std::endl;

	std::string imageAttributesStr = "[x=1280,y=720] [x=320,y=180]";

	json imageAttributes = sdptransform::parseImageAttributes(imageAttributesStr);

	std::cout << imageAttributes.dump(2) << std::endl << std::endl;

	std::cout << "sdptransform::parseSimulcastStreamList():" << std::endl << std::endl;

	std::string simulcastAttributesStr = "1,~4;2;3";

	json simulcastAttributes =
		sdptransform::parseSimulcastStreamList(simulcastAttributesStr);

	std::cout << simulcastAttributes.dump(2) << std::endl << std::endl;
}

void printWriterSection()
{
	std::cout << "### Writer" << std::endl << std::endl;

	std::cout << ">>> sdptransform::write():" << std::endl << std::endl;

	std::string newSdpStr = sdptransform::write(session);

	std::cout << newSdpStr << std::endl << std::endl;
}
