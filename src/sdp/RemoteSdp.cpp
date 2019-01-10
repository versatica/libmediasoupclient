#define MSC_CLASS "Sdp::RemoteSdp"
// #define MSC_LOG_DEV

#include "sdp/RemoteSdp.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "sdptransform.hpp"
#include <utility>

/* Sdp::RemoteSdp methods */

Sdp::RemoteSdp::RemoteSdp(json transportRemoteParameters, json sendingRtpParametersByKind)
  : transportRemoteParameters(std::move(transportRemoteParameters)),
    sendingRtpParametersByKind(std::move(sendingRtpParametersByKind))
{
	/* clang-format off */
	this->sdpGlobalFields =
	{
		{ "id",      Utils::getRandomInteger(1000, 1000000) },
		{ "version", 0 }
	};
	/* clang-format on */
}

void Sdp::RemoteSdp::UpdateTransportRemoteIceParameters(const json& remoteIceParameters)
{
	MSC_TRACE();

	this->transportRemoteParameters["iceParameters"] = remoteIceParameters;
}

std::string Sdp::RemoteSdp::CreateAnswerSdp(const json& localSdpObj)
{
	MSC_TRACE();

	if (this->transportRemoteParameters.empty())
		throw Exception("No transport remote parameters");

	const auto remoteIceParameters  = this->transportRemoteParameters["iceParameters"];
	const auto remoteIceCandidates  = this->transportRemoteParameters["iceCandidates"];
	const auto remoteDtlsParameters = this->transportRemoteParameters["dtlsParameters"];

	json sdpObj = json::object();
	std::vector<std::string> bundleMids;

	for (auto& m : localSdpObj["media"])
	{
		auto it = m.find("mid");
		if (it == m.end())
			continue;

		auto mid = (*it).get<std::string>();

		bundleMids.push_back(mid);
	}

	// Increase our SDP version.
	auto version = this->sdpGlobalFields["version"].get<uint32_t>();

	this->sdpGlobalFields["version"] = ++version;

	sdpObj["version"] = 0;

	/* clang-format off */
	sdpObj["origin"] =
	{
		{ "address",        "0.0.0.0"                        },
		{ "ipVer",          4                                },
		{ "netType",        "IN"                             },
		{ "sessionId",      this->sdpGlobalFields["id"]      },
		{ "sessionVersion", this->sdpGlobalFields["version"] },
		{ "username",       "mediasoup-client-native"        }
	};
	/* clang-format on */

	sdpObj["name"]   = "-";
	sdpObj["timing"] = { { "start", 0 }, { "stop", 0 } };

	if (remoteIceParameters.find("iceLite") != remoteIceParameters.end())
		sdpObj["icelite"] = "ice-lite";

	/* clang-format off */
	sdpObj["msidSemantic"] =
	{
		{ "semantic", "WMS" },
		{ "token",    "*"   }
	};
	/* clang-format on */

	if (!bundleMids.empty())
	{
		std::string mids;

		for (auto& mid : bundleMids)
		{
			if (!mids.empty())
				mids.append(" ");

			mids.append(mid);
		}

		/* clang-format off */
		sdpObj["groups"] =
		{
			{
				{ "type", "BUNDLE" },
				{ "mids", mids     }
			}
		};
		/* clang-format on */
	}

	sdpObj["media"] = json::array();

	// NOTE: We take the latest fingerprint.
	auto numFingerprints = remoteDtlsParameters["fingerprints"].size();

	sdpObj["fingerprint"] = {
		{ "type", remoteDtlsParameters.at("fingerprints")[numFingerprints - 1]["algorithm"] },
		{ "hash", remoteDtlsParameters.at("fingerprints")[numFingerprints - 1]["value"] }
	};

	for (auto& localMediaObj : localSdpObj["media"])
	{
		auto kind             = localMediaObj["type"].get<std::string>();
		auto codecs           = this->sendingRtpParametersByKind[kind]["codecs"];
		auto headerExtensions = this->sendingRtpParametersByKind[kind]["headerExtensions"];
		auto remoteMediaObj   = json::object();

		remoteMediaObj["type"]       = localMediaObj["type"];
		remoteMediaObj["port"]       = 7;
		remoteMediaObj["protocol"]   = "RTP/SAVPF";
		remoteMediaObj["connection"] = { { "ip", "127.0.0.1" }, { "version", 4 } };
		remoteMediaObj["mid"]        = localMediaObj["mid"];

		remoteMediaObj["iceUfrag"]   = remoteIceParameters["usernameFragment"];
		remoteMediaObj["icePwd"]     = remoteIceParameters["password"];
		remoteMediaObj["candidates"] = json::array();

		for (auto candidate : remoteIceCandidates)
		{
			auto candidateObj = json::object();

			// mediasoup does not support non rtcp-mux so candidates component is
			// always RTP (1).
			candidateObj["component"]  = 1;
			candidateObj["foundation"] = candidate["foundation"];
			candidateObj["ip"]         = candidate["ip"];
			candidateObj["port"]       = candidate["port"];
			candidateObj["priority"]   = candidate["priority"];
			candidateObj["transport"]  = candidate["protocol"];
			candidateObj["type"]       = candidate["type"];

			auto it = candidate.find("tcpType");
			if (it != candidate.end())
				candidateObj["tcptype"] = *it;

			remoteMediaObj["candidates"].push_back(candidateObj);
		}

		remoteMediaObj["endOfCandidates"] = "end-of-candidates";

		// Announce support for ICE renomination.
		// https://tools.ietf.org/html/draft-thatcher-ice-renomination
		remoteMediaObj["iceOptions"] = "renomination";

		auto role = remoteDtlsParameters["role"].get<std::string>();
		if (role == "client")
			remoteMediaObj["setup"] = "active";
		else if (role == "server")
			remoteMediaObj["setup"] = "passive";

		auto direction = localMediaObj["direction"].get<std::string>();
		if (direction == "sendrecv" || direction == "sendonly")
			remoteMediaObj["direction"] = "recvonly";
		if (direction == "recvonly" || direction == "inactive")
			remoteMediaObj["direction"] = "inactive";

		remoteMediaObj["rtp"]    = json::array();
		remoteMediaObj["rtcpFb"] = json::array();
		remoteMediaObj["fmtp"]   = json::array();

		for (auto& codec : codecs)
		{
			/* clang-format off */
			json rtp =
			{
				{ "payload", codec["payloadType"] },
				{ "codec",   codec["name"]        },
				{ "rate",    codec["clockRate"]   }
			};
			/* clang-format on */

			auto it = codec.find("channels");
			if (it != codec.end())
			{
				auto channels = (*it).get<uint8_t>();
				if (channels > 1)
					rtp["encoding"] = channels;
			}

			remoteMediaObj["rtp"].push_back(rtp);

			it = codec.find("parameters");
			if (it != codec.end())
			{
				/* clang-format off */
				json paramFmtp =
				{
					{ "payload", codec["payloadType"] }
				};
				/* clang-format on */

				std::string config;

				auto parameters = (*it).get<std::map<std::string, std::string>>();
				for (auto& kv : parameters)
				{
					if (!config.empty())
						config.append(";");

					config.append(kv.first);
					config.append("=");
					config.append(kv.second);
				}

				if (!config.empty())
				{
					paramFmtp["config"] = config;
					remoteMediaObj["fmtp"].push_back(paramFmtp);
				}
			}

			it = codec.find("rtcpFeedback");
			if (it != codec.end())
			{
				auto rtcpFeedback = *it;
				for (auto fb : rtcpFeedback)
				{
					/* clang-format off */
					remoteMediaObj["rtcpFb"].push_back(
						{
							{ "payload", codec["payloadType"] },
							{ "type",    fb["type"]           },
							{ "subtype", fb["parameter"]      }
						});
					/* clang-format on */
				}
			}
		}

		std::string payloads;

		for (auto& codec : codecs)
		{
			auto payloadType = codec["payloadType"].get<uint8_t>();

			if (!payloads.empty())
				payloads.append(" ");

			payloads.append(std::to_string(payloadType));
		}

		remoteMediaObj["payloads"] = payloads;

		remoteMediaObj["ext"] = json::array();

		for (auto ext : headerExtensions)
		{
			// Don't add a header extension if not present in the offer.
			auto it = localMediaObj.find("ext");
			if (it == localMediaObj.end())
				continue;

			auto localExts = *it;
			it             = find_if(localExts.begin(), localExts.end(), [&ext](const json& localExt) {
        return localExt["uri"] == ext["uri"];
      });

			if (it == localExts.end())
				continue;

			/* clang-format off */
			remoteMediaObj["ext"].push_back(
				{
					{ "uri",   ext["uri"] },
					{ "value", ext["id"]  }
				});
			/* clang-format on */
		}

		remoteMediaObj["rtcpMux"]   = "rtcp-mux";
		remoteMediaObj["rtcpRsize"] = "rtcp-rsize";

		// Push it.
		sdpObj["media"].push_back(remoteMediaObj);
	}

	auto sdp = sdptransform::write(sdpObj);

	return sdp;
}

std::string Sdp::RemoteSdp::CreateOfferSdp(const json& receiverInfos)
{
	MSC_TRACE();

	if (this->transportRemoteParameters.empty())
		throw Exception("No transport remote parameters");

	const auto remoteIceParameters  = this->transportRemoteParameters["iceParameters"];
	const auto remoteIceCandidates  = this->transportRemoteParameters["iceCandidates"];
	const auto remoteDtlsParameters = this->transportRemoteParameters["dtlsParameters"];

	json sdpObj = json::object();
	json mids   = json::array();

	for (auto& info : receiverInfos)
	{
		auto it = info.find("mid");
		if (it == info.end())
			continue;

		mids.push_back((*it).get<std::string>());
	}

	// Increase our SDP version.
	auto version = this->sdpGlobalFields["version"].get<uint32_t>();

	this->sdpGlobalFields["version"] = ++version;

	sdpObj["version"] = 0;

	/* clang-format off */
	sdpObj["origin"] =
	{
		{ "address",        "0.0.0.0"                        },
		{ "ipVer",          4                                },
		{ "netType",        "IN"                             },
		{ "sessionId",      this->sdpGlobalFields["id"]      },
		{ "sessionVersion", this->sdpGlobalFields["version"] },
		{ "username",       "mediasoup-client-native"        }
	};
	/* clang-format on */

	sdpObj["name"]   = "-";
	sdpObj["timing"] = { { "start", 0 }, { "stop", 0 } };

	if (remoteIceParameters.find("iceLite") != remoteIceParameters.end())
		sdpObj["icelite"] = "ice-lite";

	/* clang-format off */
	sdpObj["msidSemantic"] =
	{
		{ "semantic", "WMS" },
		{ "token",    "*"   }
	};
	/* clang-format on */

	if (!mids.empty())
	{
		std::string midsStr;

		for (auto& mid : mids)
		{
			if (!midsStr.empty())
				midsStr.append(" ");

			midsStr.append(mid.get<std::string>());
		}

		/* clang-format off */
		sdpObj["groups"] =
		{
			{
				{ "type", "BUNDLE" },
				{ "mids", midsStr  }
			}
		};
		/* clang-format on */
	}

	sdpObj["media"] = json::array();

	// NOTE: We take the latest fingerprint.
	auto numFingerprints = remoteDtlsParameters["fingerprints"].size();

	sdpObj["fingerprint"] = {
		{ "type", remoteDtlsParameters.at("fingerprints")[numFingerprints - 1]["algorithm"] },
		{ "hash", remoteDtlsParameters.at("fingerprints")[numFingerprints - 1]["value"] }
	};

	for (auto& info : receiverInfos)
	{
		auto closed           = info["closed"].get<bool>();
		auto kind             = info["kind"].get<std::string>();
		auto codecs           = json::array();
		auto headerExtensions = json::array();
		auto remoteMediaObj   = json::object();

		if (kind != "application")
		{
			codecs           = this->sendingRtpParametersByKind[kind]["codecs"];
			headerExtensions = this->sendingRtpParametersByKind[kind]["headerExtensions"];

			remoteMediaObj["type"]       = kind;
			remoteMediaObj["port"]       = 7;
			remoteMediaObj["protocol"]   = "RTP/SAVPF";
			remoteMediaObj["connection"] = { { "ip", "127.0.0.1" }, { "version", 4 } };
			remoteMediaObj["mid"]        = info["mid"];

			std::string msid;
			msid.append(info["streamId"].get<std::string>());
			msid.append(" ");
			msid.append(info["trackId"].get<std::string>());

			remoteMediaObj["msid"] = msid;
		}
		else
		{
			remoteMediaObj["type"]       = kind;
			remoteMediaObj["port"]       = 7;
			remoteMediaObj["protocol"]   = "DTLS/SCTP";
			remoteMediaObj["connection"] = { { "ip", "127.0.0.1" }, { "version", 4 } };
			remoteMediaObj["mid"]        = info["mid"];
		}

		remoteMediaObj["iceUfrag"]   = remoteIceParameters["usernameFragment"];
		remoteMediaObj["icePwd"]     = remoteIceParameters["password"];
		remoteMediaObj["candidates"] = json::array();

		for (auto candidate : remoteIceCandidates)
		{
			auto candidateObj = json::object();

			// mediasoup does not support non rtcp-mux so candidates component is
			// always RTP (1).
			candidateObj["component"]  = 1;
			candidateObj["foundation"] = candidate["foundation"];
			candidateObj["ip"]         = candidate["ip"];
			candidateObj["port"]       = candidate["port"];
			candidateObj["priority"]   = candidate["priority"];
			candidateObj["transport"]  = candidate["protocol"];
			candidateObj["type"]       = candidate["type"];

			auto it = candidate.find("tcpType");
			if (it != candidate.end())
				candidateObj["tcptype"] = *it;

			remoteMediaObj["candidates"].push_back(candidateObj);
		}

		remoteMediaObj["endOfCandidates"] = "end-of-candidates";

		// Announce support for ICE renomination.
		// https://tools.ietf.org/html/draft-thatcher-ice-renomination
		remoteMediaObj["iceOptions"] = "renomination";

		remoteMediaObj["setup"] = "actpass";

		if (kind != "application")
		{
			if (!closed)
				remoteMediaObj["direction"] = "sendonly";
			else
				remoteMediaObj["direction"] = "inactive";

			remoteMediaObj["rtp"]    = json::array();
			remoteMediaObj["rtcpFb"] = json::array();
			remoteMediaObj["fmtp"]   = json::array();

			for (auto& codec : codecs)
			{
				/* clang-format off */
				json rtp =
				{
					{ "payload", codec["payloadType"] },
					{ "codec",   codec["name"]        },
					{ "rate",    codec["clockRate"]   }
				};
				/* clang-format on */

				auto it = codec.find("channels");
				if (it != codec.end())
				{
					auto channels = (*it).get<uint8_t>();
					if (channels > 1)
						rtp["encoding"] = channels;
				}

				remoteMediaObj["rtp"].push_back(rtp);

				it = codec.find("parameters");
				if (it != codec.end())
				{
					/* clang-format off */
					json paramFmtp =
					{
						{ "payload", codec["payloadType"] }
					};
					/* clang-format on */

					std::string config;

					auto parameters = (*it).get<std::map<std::string, std::string>>();
					for (auto& kv : parameters)
					{
						if (!config.empty())
							config.append(";");

						config.append(kv.first);
						config.append("=");
						config.append(kv.second);
					}

					if (!config.empty())
					{
						paramFmtp["config"] = config;
						remoteMediaObj["fmtp"].push_back(paramFmtp);
					}
				}

				it = codec.find("rtcpFeedback");
				if (it != codec.end())
				{
					auto rtcpFeedback = *it;
					for (auto fb : rtcpFeedback)
					{
						/* clang-format off */
						remoteMediaObj["rtcpFb"].push_back(
							{
								{ "payload", codec["payloadType"] },
								{ "type",    fb["type"]           },
								{ "subtype", fb["parameter"]      }
							});
						/* clang-format on */
					}
				}
			}

			std::string payloads;

			for (auto& codec : codecs)
			{
				auto payloadType = codec["payloadType"].get<uint8_t>();

				if (!payloads.empty())
					payloads.append(" ");

				payloads.append(std::to_string(payloadType));
			}

			remoteMediaObj["payloads"] = payloads;
			remoteMediaObj["ext"]      = json::array();

			for (auto ext : headerExtensions)
			{
				// Ignore MID RTP extension for receiving media.
				if ("urn:ietf:params:rtp-hdrext:sdes:mid" == ext["uri"].get<std::string>())
					continue;

				/* clang-format off */
				remoteMediaObj["ext"].push_back(
					{
						{ "uri",   ext["uri"] },
						{ "value", ext["id"]  }
					});
				/* clang-format on */
			}

			remoteMediaObj["rtcpMux"]   = "rtcp-mux";
			remoteMediaObj["rtcpRsize"] = "rtcp-rsize";

			if (!closed)
			{
				remoteMediaObj["ssrcs"]      = json::array();
				remoteMediaObj["ssrcGroups"] = json::array();

				/* clang-format off */
				remoteMediaObj["ssrcs"].push_back(
				{
					{ "id",        info["ssrc"]  },
					{ "attribute", "cname"       },
					{ "value",     info["cname"] }
				});
				/* clang-format on */

				auto it = info.find("rtxSsrc");
				if (it != info.end())
				{
					auto rtcSsrc = *it;

					/* clang-format off */
					remoteMediaObj["ssrcs"].push_back(
						{
							{ "id",        rtcSsrc         },
							{ "attribute", "cname"         },
							{ "value",     info["cname"]   }
						});
					/* clang-format on */

					std::string ssrcs;
					ssrcs.append(info["ssrc"].get<std::string>());
					ssrcs.append(info["rtcSsrc"].get<std::string>());

					// Associate original and retransmission SSRC.
					/* clang-format off */
					remoteMediaObj["ssrcGroups"].push_back(
						{
							{ "semantics", "FID" },
							{ "ssrcs"    , ssrcs }
						});
					/* clang-format on */
				}
			}
		}
		// DataChannel.
		else
		{
			remoteMediaObj["payloads"] = 5000;

			/* clang-format off */
			remoteMediaObj["sctpmap"] =
			{
				{ "app",            "webrtc-datachannel" },
				{ "maxMessageSize",  256 },
				{ "sctpmapNumber",   5000 }
			};
			/* clang-format on */
		}

		// Push it.
		sdpObj["media"].push_back(remoteMediaObj);
	}

	auto sdp = sdptransform::write(sdpObj);

	return sdp;
}
