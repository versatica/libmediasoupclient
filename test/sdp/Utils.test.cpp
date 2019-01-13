#include "catch.hpp"
#include "helpers.hpp"
#include "sdp/Utils.hpp"

using namespace mediasoupclient;

TEST_CASE("Sdp::Utils", "[Sdp][Utils]")
{
	SECTION("ExtractDtlsParameters")
	{
		auto sdp     = helpers::readFile("test/sdp/data/jssip.sdp");
		auto session = sdptransform::parse(sdp);

		auto dtlsParameters = Sdp::Utils::extractDtlsParameters(session);

		REQUIRE(dtlsParameters.at("role") == "auto");

		auto& fingerprints = dtlsParameters["fingerprints"];

		REQUIRE(fingerprints.size() == 1);
		REQUIRE(
		  fingerprints[0] ==
		  R"({
					"algorithm" : "sha-256",
					"value"		 : "79:14:AB:AB:93:7F:07:E8:91:1A:11:16:36:D0:11:66:C4:4F:31:A0:74:46:65:58:70:E5:09:95:48:F4:4B:D9"
				})"_json);
	}
}
