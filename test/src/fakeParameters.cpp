#include "fakeParameters.hpp"
#include "Utils.hpp"
#include <json.hpp>
#include <string>

json generateRouterRtpCapabilities()
{
	auto codecs           = json::array();
	auto headerExtensions = json::array();
	auto fecMechanisms    = json::array();

	codecs = R"(
	[
		{
			"mimeType"             : "audio/opus",
			"kind"                 : "audio",
			"clockRate"            : 48000,
			"preferredPayloadType" : 100,
			"channels"             : 2,
			"rtcpFeedback"         : [],
			"parameters"           :
			{
				"useinbandfec" : 1
			}
		},
		{
			"mimeType"             : "video/VP8",
			"kind"                 : "video",
			"clockRate"            : 90000,
			"preferredPayloadType" : 101,
			"rtcpFeedback"         :
			[
				{ "type": "nack" },
				{ "type": "nack", "parameter": "pli"  },
				{ "type": "nack", "parameter": "sli"  },
				{ "type": "nack", "parameter": "rpsi" },
				{ "type": "nack", "parameter": "app"  },
				{ "type": "ccm",  "parameter": "fir"  },
				{ "type": "goog-remb" }
			],
			"parameters" :
			{
				"x-google-start-bitrate" : "1500"
			}
		},
		{
			"mimeType"             : "video/rtx",
			"kind"                 : "video",
			"clockRate"            : 90000,
			"preferredPayloadType" : 102,
			"rtcpFeedback"         : [],
			"parameters"           :
			{
				"apt" : 101
			}
		},
		{
			"mimeType"             : "video/H264",
			"kind"                 : "video",
			"clockRate"            : 90000,
			"preferredPayloadType" : 103,
			"rtcpFeedback"         :
			[
				{ "type": "nack" },
				{ "type": "nack", "parameter": "pli"  },
				{ "type": "nack", "parameter": "sli"  },
				{ "type": "nack", "parameter": "rpsi" },
				{ "type": "nack", "parameter": "app"  },
				{ "type": "ccm",  "parameter": "fir"  },
				{ "type": "goog-remb" }
			],
			"parameters" :
			{
				"level-asymmetry-allowed" : 1,
				"packetization-mode"      : 1,
				"profile-level-id"        : "42e01f"
			}
		},
		{
			"mimeType"             : "video/rtx",
			"kind"                 : "video",
			"clockRate"            : 90000,
			"preferredPayloadType" : 104,
			"rtcpFeedback"         : [],
			"parameters"           :
			{
				"apt" : 103
			}
		}
	])"_json;

	headerExtensions = R"(
	[
		{
			"kind"             : "audio",
			"uri"              : "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
			"preferredId"      : 1,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "video",
			"uri"              : "urn:ietf:params:rtp-hdrext:toffset",
			"preferredId"      : 2,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "audio",
			"uri"              : "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
			"preferredId"      : 3,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "video",
			"uri"              : "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
			"preferredId"      : 3,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "video",
			"uri"              : "urn:3gpp:video-orientation",
			"preferredId"      : 4,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "audio",
			"uri"              : "urn:ietf:params:rtp-hdrext:sdes:mid",
			"preferredId"      : 5,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "video",
			"uri"              : "urn:ietf:params:rtp-hdrext:sdes:mid",
			"preferredId"      : 5,
			"preferredEncrypt" : false
		},
		{
			"kind"             : "video",
			"uri"              : "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id",
			"preferredId"      : 6,
			"preferredEncrypt" : false
		}
	])"_json;

	json capabilities = { { "codecs", codecs },
		                    { "headerExtensions", headerExtensions },
		                    { "fecMechanisms", fecMechanisms } };

	return capabilities;
};

json generateRtpParametersByKind()
{
	/* clang-format off */
	json rtpParametersByKind =
	{
		{
			"audio",
			{
				{ "codecs",           json::array() },
				{ "headerExtensions", json::array() }
			}
		},
		{
			"video",
			{
				{ "codecs",           json::array() },
				{ "headerExtensions", json::array() }
			}
		}
	};
	/* clang-format on */

	auto codecs = generateRouterRtpCapabilities()["codecs"];

	for (auto& codec : codecs)
	{
		codec["payloadType"] = codec["preferredPayloadType"];
		codec.erase("preferredPayloadType");

		auto kind = codec["kind"].get<std::string>();

		if (kind == "audio")
			rtpParametersByKind["audio"]["codecs"].push_back(codec);
		else if (kind == "video")
			rtpParametersByKind["video"]["codecs"].push_back(codec);
	}

	auto headerExtensions = generateRouterRtpCapabilities()["headerExtensions"];
	for (auto& ext : headerExtensions)
	{
		ext["id"] = ext["preferredId"];
		ext.erase("preferredId");

		auto kind = ext["kind"].get<std::string>();

		if (kind == "audio")
			rtpParametersByKind["audio"]["headerExtensions"].push_back(ext);
		else if (kind == "video")
			rtpParametersByKind["video"]["headerExtensions"].push_back(ext);
	}

	return rtpParametersByKind;
}

json generateLocalDtlsParameters()
{
	return R"(
	{
		"fingerprints" :
		[
			{
				"algorithm" : "sha-256",
				"value"     : "82:5A:68:3D:36:C3:0A:DE:AF:E7:32:43:D2:88:83:57:AC:2D:65:E5:80:C4:B6:FB:AF:1A:A0:21:9F:6D:0C:AD"
			}
		],
		"role" : "auto"
	})"_json;
};

json generateTransportRemoteParameters()
{
	auto json = R"(
	{
		"id"            : "",
		"iceParameters" :
		{
			"iceLite"          : true,
			"password"         : "yku5ej8nvfaor28lvtrabcx0wkrpkztz",
			"usernameFragment" : "h3hk1iz6qqlnqlne"
		},
		"iceCandidates" :
		[
			{
				"family"     : "ipv4",
				"foundation" : "udpcandidate",
				"ip"         : "9.9.9.9",
				"port"       : 40533,
				"priority"   : 1078862079,
				"protocol"   : "udp",
				"type"       : "host"
			},
			{
				"family"     : "ipv6",
				"foundation" : "udpcandidate",
				"ip"         : "9:9:9:9:9:9",
				"port"       : 41333,
				"priority"   : 1078862089,
				"protocol"   : "udp",
				"type"       : "host"
			}
		],
		"dtlsParameters" :
		{
			"fingerprints" :
			[
				{
					"algorithm" : "sha-256",
					"value"     : "A9:F4:E0:D2:74:D3:0F:D9:CA:A5:2F:9F:7F:47:FA:F0:C4:72:DD:73:49:D0:3B:14:90:20:51:30:1B:90:8E:71"
				},
				{
					"algorithm" : "sha-384",
					"value"     : "03:D9:0B:87:13:98:F6:6D:BC:FC:92:2E:39:D4:E1:97:32:61:30:56:84:70:81:6E:D1:82:97:EA:D9:C1:21:0F:6B:C5:E7:7F:E1:97:0C:17:97:6E:CF:B3:EF:2E:74:B0"
				},
				{
					"algorithm" : "sha-512",
					"value"     : "84:27:A4:28:A4:73:AF:43:02:2A:44:68:FF:2F:29:5C:3B:11:9A:60:F4:A8:F0:F5:AC:A0:E3:49:3E:B1:34:53:A9:85:CE:51:9B:ED:87:5E:B8:F4:8E:3D:FA:20:51:B8:96:EE:DA:56:DC:2F:5C:62:79:15:23:E0:21:82:2B:2C"
				}
			],
			"role" : "auto"
		},
		"sctpParameters" :
		{
			"port"              : 5000,
			"OS"                : 1024,
			"MIS"               : 1024,
			"numStreams"        : 2048,
			"maxMessageSize"    : 2000000
		}
	})"_json;

	json["id"] = mediasoupclient::Utils::getRandomString(12);

	return json;
};

std::string generateProducerRemoteId()
{
	return mediasoupclient::Utils::getRandomString(12);
};

json generateConsumerRemoteParameters(const std::string& codecMimeType)
{
	if (codecMimeType == "audio/opus")
	{
		auto json = R"(
		{
			"producerId"    : "",
			"id"            : "",
			"kind"          : "audio",
			"rtpParameters" :
			{
				"codecs" :
				[
					{
						"mimeType"     : "audio/opus",
						"clockRate"    : 48000,
						"payloadType"  : 100,
						"channels"     : 2,
						"rtcpFeedback" : [],
						"parameters"   :
						{
							"useinbandfec" : "1"
						}
					}
				],
				"encodings" :
				[
					{
						"ssrc" : 0
					}
				],
				"headerExtensions" :
				[
					{
						"uri" : "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
						"id"  : 1
					}
				],
				"rtcp" :
				{
					"cname"       : "",
					"reducedSize" : true,
					"mux"         : true
				}
			}
		})"_json;

		json["producerId"] = mediasoupclient::Utils::getRandomString(12);
		json["id"]         = mediasoupclient::Utils::getRandomString(12);
		json["rtpParameters"]["encodings"][0]["ssrc"] =
		  mediasoupclient::Utils::getRandomInteger(1000000, 1999999);
		json["rtpParameters"]["rtcp"]["cname"] = mediasoupclient::Utils::getRandomString(16);

		return json;
	}
	else if (codecMimeType == "audio/ISAC")
	{
		auto json = R"(
		{
			"producerId"    : "",
			"id"            : "",
			"kind"          : "audio",
			"rtpParameters" :
			{
				"codecs" :
				[
					{
						"mimeType"     : "audio/ISAC",
						"clockRate"    : 16000,
						"payloadType"  : 111,
						"channels"     : 1,
						"rtcpFeedback" : [],
						"parameters"   : {}
					}
				],
				"encodings" :
				[
					{
						"ssrc" : 0
					}
				],
				"headerExtensions" :
				[
					{
						"uri" : "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
						"id"  : 1
					}
				],
				"rtcp" :
				{
					"cname"       : "",
					"reducedSize" : true,
					"mux"         : true
				}
			}
		})"_json;

		json["producerId"] = mediasoupclient::Utils::getRandomString(12);
		json["id"]         = mediasoupclient::Utils::getRandomString(12);
		json["rtpParameters"]["encodings"][0]["ssrc"] =
		  mediasoupclient::Utils::getRandomInteger(1000000, 1999999);
		json["rtpParameters"]["rtcp"]["cname"] = mediasoupclient::Utils::getRandomString(16);

		return json;
	}
	else if (codecMimeType == "video/VP8")
	{
		auto json = R"(
		{
			"producerId"    : "",
			"id"            : "",
			"kind"          : "video",
			"rtpParameters" :
			{
				"codecs" :
				[
					{
						"mimeType"     : "video/VP8",
						"clockRate"    : 90000,
						"payloadType"  : 101,
						"rtcpFeedback" :
						[
							{ "type": "nack" },
							{ "type": "nack", "parameter": "pli" },
							{ "type": "nack", "parameter": "sli" },
							{ "type": "nack", "parameter": "rpsi" },
							{ "type": "nack", "parameter": "app" },
							{ "type": "ccm",  "parameter": "fir" },
							{ "type": "goog-remb" }
						],
						"parameters" :
						{
							"x-google-start-bitrate" : "1500"
						}
					},
					{
						"mimeType"     : "video/rtx",
						"clockRate"    : 90000,
						"payloadType"  : 102,
						"rtcpFeedback" : [],
						"parameters"   :
						{
							"apt" : 101
						}
					}
				],
				"encodings" :
				[
					{
						"ssrc" : 0,
						"rtx"  :
						{
							"ssrc" : 0
						}
					}
				],
				"headerExtensions" :
				[
					{
						"uri" : "urn:ietf:params:rtp-hdrext:toffset",
						"id"  : 2
					},
					{
						"uri" : "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
						"id"  : 3
					}
				],
				"rtcp" :
				{
					"cname"       : "",
					"reducedSize" : true,
					"mux"         : true
				}
			}
		})"_json;

		json["producerId"] = mediasoupclient::Utils::getRandomString(12);
		json["id"]         = mediasoupclient::Utils::getRandomString(12);
		json["rtpParameters"]["encodings"][0]["ssrc"] =
		  mediasoupclient::Utils::getRandomInteger(2000000, 2999999);
		json["rtpParameters"]["encodings"][0]["rtx"]["ssrc"] =
		  mediasoupclient::Utils::getRandomInteger(3000000, 3999999);
		json["rtpParameters"]["rtcp"]["cname"] = mediasoupclient::Utils::getRandomString(16);

		return json;
	}

	return json::object();
};

json generateIceServers()
{
	auto json = R"(
	[
		{
			"urls"       : [
				"turn:t1.server.com",
				"turn:t2.server.com"
			],
			"username"   : "fakeuser",
			"credential" : "fakepass"
		},
		{
			"urls" : "stun:s.server.com"
		}
	])"_json;
	return json;
};
