#include "helpers.hpp"
#include "sdp/Utils.hpp"
#include <catch.hpp>

TEST_CASE("Sdp::Utils", "[Sdp][Utils]")
{
	SECTION("extractRtpCapabilities")
	{
		auto sdp             = helpers::readFile("test/data/audio_video.sdp");
		auto session         = sdptransform::parse(sdp);
		auto rtpCapabilities = mediasoupclient::Sdp::Utils::extractRtpCapabilities(session);

		auto codecs = rtpCapabilities.at("codecs");

		for (const auto& codec : rtpCapabilities["codecs"])
		{
			// Verify that 'profile-id' is a number.
			if (codec["parameters"].contains("profile-id"))
			{
				REQUIRE_NOTHROW(codec["parameters"]["profile-id"].get<int>());
			}
		}
	}

	SECTION("extractDtlsParameters")
	{
		auto sdp            = helpers::readFile("test/data/jssip.sdp");
		auto session        = sdptransform::parse(sdp);
		auto dtlsParameters = mediasoupclient::Sdp::Utils::extractDtlsParameters(session);

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

	SECTION("getRtpEncodings respect the given SSRC order")
	{
		auto offerMediaObject = R"({
			"ssrcs":
			[
				{
				  "attribute": "cname",
				  "id": 3142507807,
				  "value": "xP/I5Utgvn9wJsho"
				},
				{
				  "attribute": "msid",
				  "id": 3142507807,
				  "value": "0 audio-track-id"
				},
				{
				  "attribute": "mslabel",
				  "id": 3142507807,
				  "value": "0"
				},
				{
				  "attribute": "label",
				  "id": 3142507807,
				  "value": "audio-track-id"
				},
				{
				  "attribute": "cname",
				  "id": 3142507806,
				  "value": "xP/I5Utgvn9wJsho"
				},
				{
				  "attribute": "msid",
				  "id": 3142507806,
				  "value": "0 audio-track-id"
				},
				{
				  "attribute": "mslabel",
				  "id": 3142507806,
				  "value": "0"
				},
				{
				  "attribute": "label",
				  "id": 3142507806,
				  "value": "audio-track-id"
				}
			],
		"type": "audio"
})"_json;

		auto rtpEncodings = mediasoupclient::Sdp::Utils::getRtpEncodings(offerMediaObject);

		REQUIRE(rtpEncodings[0]["ssrc"] == 3142507807);
		REQUIRE(rtpEncodings[1]["ssrc"] == 3142507806);
	}
}
