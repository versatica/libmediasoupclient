#define MSC_CLASS "ortc"
// #define MSC_LOG_DEV

#include "ortc.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "media/base/h264_profile_level_id.h"
#include <algorithm> // std::find_if
#include <regex>
#include <string>

using json = nlohmann::json;

static bool isRtxCodec(const json& codec)
{
	static const std::regex regex(".+/rtx$", std::regex_constants::ECMAScript);

	std::smatch match;
	auto name = codec["mimeType"].get<std::string>();

	return std::regex_match(name, match, regex);
}

namespace mediasoupclient
{
namespace ortc
{
	static uint8_t getH264PacketizationMode(const json& codec)
	{
		MSC_TRACE();

		auto jsonParametersIt = codec.find("parameters");
		if (jsonParametersIt == codec.end())
			return 0;

		auto parameters              = *jsonParametersIt;
		auto jsonPacketizationModeIt = parameters.find("packetization-mode");
		if (jsonPacketizationModeIt == parameters.end() || !jsonPacketizationModeIt->is_number())
			return 0;

		return jsonPacketizationModeIt->get<uint8_t>();
	}

	static uint8_t getH264LevelAssimetryAllowed(const json& codec)
	{
		MSC_TRACE();

		auto jsonParametersIt = codec.find("parameters");
		if (jsonParametersIt == codec.end())
			return 0;

		auto parameters                  = *jsonParametersIt;
		auto jsonLevelAssimetryAllowedIt = parameters.find("level-assimetry-allowed");
		if (jsonLevelAssimetryAllowedIt == parameters.end() || !jsonLevelAssimetryAllowedIt->is_number_unsigned())
			return 0;

		return jsonLevelAssimetryAllowedIt->get<uint8_t>();
	}

	static bool matchCodecs(json& aCodec, const json& bCodec)
	{
		MSC_TRACE();

		auto aMimeType = aCodec["mimeType"].get<std::string>();
		auto bMimeType = bCodec["mimeType"].get<std::string>();

		std::transform(aMimeType.begin(), aMimeType.end(), aMimeType.begin(), ::tolower);
		std::transform(bMimeType.begin(), bMimeType.end(), bMimeType.begin(), ::tolower);

		if (aMimeType != bMimeType)
			return false;

		if (aCodec["clockRate"] != bCodec["clockRate"])
			return false;

		auto jsonChannelsAIt = aCodec.find("channels");
		auto jsonChannelsBIt = bCodec.find("channels");

		if (jsonChannelsAIt == aCodec.end() && jsonChannelsBIt != bCodec.end())
			return false;

		if (jsonChannelsAIt != aCodec.end() && jsonChannelsBIt == bCodec.end())
			return false;

		if (jsonChannelsAIt != aCodec.end() && aCodec["channels"] != bCodec["channels"])
			return false;

		// Match H264 parameters.
		if (aMimeType == "video/h264")
		{
			auto aPacketizationMode = getH264PacketizationMode(aCodec);
			auto bPacketizationMode = getH264PacketizationMode(bCodec);

			if (aPacketizationMode != bPacketizationMode)
				return false;

			webrtc::H264::CodecParameterMap aParameters;
			webrtc::H264::CodecParameterMap bParameters;

			// Check H264 profile.
			aParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(aCodec));
			aParameters["packetization-mode"]      = std::to_string(aPacketizationMode);
			aParameters["profile-level-id"] = aCodec["parameters"]["profile-level-id"].get<std::string>();

			bParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(bCodec));
			bParameters["packetization-mode"]      = std::to_string(bPacketizationMode);
			bParameters["profile-level-id"] = bCodec["parameters"]["profile-level-id"].get<std::string>();

			if (!webrtc::H264::IsSameH264Profile(aParameters, bParameters))
				return false;

			webrtc::H264::CodecParameterMap newParameters;

			webrtc::H264::GenerateProfileLevelIdForAnswer(aParameters, bParameters, &newParameters);

			auto profileLevelIdIt = newParameters.find("profile-level-id");
			if (profileLevelIdIt != newParameters.end())
				aCodec["parameters"]["profile-level-id"] = profileLevelIdIt->second;
			else
				aCodec["parameters"].erase("profile-level-id");
		}

		return true;
	}

	static bool matchHeaderExtensions(const json& aExt, const json& bExt)
	{
		MSC_TRACE();

		if (aExt["kind"] != bExt["kind"])
			return false;

		return aExt["uri"] == bExt["uri"];
	}

	static json reduceRtcpFeedback(const json& codecA, const json& codecB)
	{
		MSC_TRACE();

		auto reducedRtcpFeedback = json::array();

		auto jsonRtcpFeedbackAIt = codecA["rtcpFeedback"];
		auto jsonRtcpFeedbackBIt = codecB["rtcpFeedback"];

		for (auto& aFb : jsonRtcpFeedbackAIt)
		{
			auto jsonRtcpFeedbackIt =
			  std::find_if(jsonRtcpFeedbackBIt.begin(), jsonRtcpFeedbackBIt.end(), [&aFb](const json& bFb) {
				  if (aFb["type"] != bFb["type"])
					  return false;

				  auto jsonParameterAIt = aFb.find("parameter");
				  auto jsonParameterBIt = bFb.find("parameter");

				  if (jsonParameterAIt == aFb.end() && jsonParameterBIt != bFb.end())
					  return false;

				  if (jsonParameterAIt != aFb.end() && jsonParameterBIt == bFb.end())
					  return false;

				  if (jsonParameterAIt == aFb.end())
					  return true;

				  return (*jsonParameterAIt) == (*jsonParameterBIt);
			  });

			if (jsonRtcpFeedbackIt != jsonRtcpFeedbackBIt.end())
				reducedRtcpFeedback.push_back(*jsonRtcpFeedbackIt);
		}

		return reducedRtcpFeedback;
	}

	/**
	 * Generate extended RTP capabilities for sending and receiving.
	 */
	json getExtendedRtpCapabilities(json& localCaps, const json& remoteCaps)
	{
		MSC_TRACE();

		/* clang-format off */
		json extendedRtpCapabilities =
		{
			{ "codecs",           json::array() },
			{ "headerExtensions", json::array() },
			{ "fecMechanisms",    json::array() }
		};
		/* clang-format on */

		static const std::regex regex("^(audio|video)/(.+)", std::regex_constants::ECMAScript);

		std::smatch match;

		// Match media codecs and keep the order preferred by remoteCaps.
		for (auto& remoteCodec : remoteCaps["codecs"])
		{
			/* clang-format off */
			if (
				!remoteCodec.is_object() ||
				remoteCodec.find("mimeType") == remoteCodec.end() ||
				!remoteCodec["mimeType"].is_string()
			)
			/* clang-format on */
			{
				throw Exception("Invalid remote capabilitiy codec");
			}

			auto mimeType = remoteCodec["mimeType"].get<std::string>();
			if (!std::regex_match(mimeType, match, regex))
			{
				throw Exception("Invalid remote capabilitiy codec");
			}

			if (isRtxCodec(remoteCodec))
				continue;

			json& localCodecs = localCaps["codecs"];

			auto jsonLocalCodecIt =
			  std::find_if(localCodecs.begin(), localCodecs.end(), [&remoteCodec](json& localCodec) {
				  return matchCodecs(localCodec, remoteCodec);
			  });

			if (jsonLocalCodecIt != localCodecs.end())
			{
				auto localCodec = *jsonLocalCodecIt;

				/* clang-format off */
				json extendedCodec =
				{
					{ "mimeType",             localCodec["mimeType"]                      },
					{ "kind",                 localCodec["kind"]                          },
					{ "clockRate",            localCodec["clockRate"]                     },
					{ "localPayloadType",     localCodec["preferredPayloadType"]          },
					{ "localRtxPayloadType",  nullptr                                     },
					{ "remotePayloadType",    remoteCodec["preferredPayloadType"]         },
					{ "remoteRtxPayloadType", nullptr                                     },
					{ "rtcpFeedback",         reduceRtcpFeedback(localCodec, remoteCodec) },
					{ "localParameters",      localCodec["parameters"]                    },
					{ "remoteParameters",     remoteCodec["parameters"]                   }
				};
				/* clang-format on */

				auto jsonChannelsIt = localCodec.find("channels");
				if (jsonChannelsIt != localCodec.end() && jsonChannelsIt->is_number())
				{
					auto channels = jsonChannelsIt->get<uint8_t>();
					if (channels > 0)
						extendedCodec["channels"] = channels;
				}

				extendedRtpCapabilities["codecs"].push_back(extendedCodec);
			}
		}

		// Match RTX codecs.
		json& extendedCodecs = extendedRtpCapabilities["codecs"];
		for (json& extendedCodec : extendedCodecs)
		{
			auto localCodecs      = localCaps["codecs"];
			auto jsonLocalCodecIt = std::find_if(
			  localCodecs.begin(), localCodecs.end(), [&extendedCodec](const json& localCodec) {
				  return isRtxCodec(localCodec) &&
				         localCodec["parameters"]["apt"] == extendedCodec["localPayloadType"];
			  });

			if (jsonLocalCodecIt == localCodecs.end())
				continue;

			auto matchingLocalRtxCodec = *jsonLocalCodecIt;

			auto remoteCodecs      = remoteCaps["codecs"];
			auto jsonRemoteCodecIt = std::find_if(
			  remoteCodecs.begin(), remoteCodecs.end(), [&extendedCodec](const json& remoteCodec) {
				  return isRtxCodec(remoteCodec) &&
				         remoteCodec["parameters"]["apt"] == extendedCodec["remotePayloadType"];
			  });

			if (jsonRemoteCodecIt == remoteCodecs.end())
				continue;

			auto matchingRemoteRtxCodec = *jsonRemoteCodecIt;

			extendedCodec["localRtxPayloadType"]  = matchingLocalRtxCodec["preferredPayloadType"];
			extendedCodec["remoteRtxPayloadType"] = matchingRemoteRtxCodec["preferredPayloadType"];
		}

		// Match header extensions.
		auto remoteExts = remoteCaps["headerExtensions"];
		for (auto& remoteExt : remoteExts)
		{
			auto localExts = localCaps["headerExtensions"];

			auto jsonLocalExtIt =
			  std::find_if(localExts.begin(), localExts.end(), [&remoteExt](const json& localExt) {
				  return matchHeaderExtensions(localExt, remoteExt);
			  });

			if (jsonLocalExtIt == localExts.end())
				continue;

			auto matchingLocalExt = *jsonLocalExtIt;

			/* clang-format off */
			json extendedExt =
			{
				{ "kind",      remoteExt["kind"]               },
				{ "uri",       remoteExt["uri"]                },
				{ "sendId",    matchingLocalExt["preferredId"] },
				{ "recvId",    remoteExt["preferredId"]        },
				{ "direction", remoteExt["sendrecv"]           }
			};
			/* clang-format on */

			auto jsonRemoteExtDirectionIt = remoteExt.find("direction");

			if (jsonRemoteExtDirectionIt != remoteExt.end() && jsonRemoteExtDirectionIt->is_string())
			{
				auto remoteExtDirection = jsonRemoteExtDirectionIt->get<std::string>();

				if (remoteExtDirection == "recvonly")
					extendedExt["direction"] = "sendonly";
				else if (remoteExtDirection == "sendonly")
					extendedExt["direction"] = "recvonly";
				else if (remoteExtDirection == "inactive")
					extendedExt["direction"] = "inactive";
			}

			extendedRtpCapabilities["headerExtensions"].push_back(extendedExt);
		}

		return extendedRtpCapabilities;
	}

	/**
	 * Generate RTP capabilities for receiving media based on the given extended
	 * RTP capabilities.
	 */
	json getRecvRtpCapabilities(const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

		/* clang-format off */
		json rtpCapabilities =
		{
			{ "codecs",           json::array() },
			{ "headerExtensions", json::array() },
			{ "fecMechanisms",    json::array() }
		};
		/* clang-format on */

		for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
		{
			/* clang-format off */
			json codec =
			{
				{ "mimeType",             extendedCodec["mimeType"]          },
				{ "kind",                 extendedCodec["kind"]              },
				{ "clockRate",            extendedCodec["clockRate"]         },
				{ "preferredPayloadType", extendedCodec["remotePayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]      },
				{ "parameters",           extendedCodec["localParameters"]   }
			};
			/* clang-format on */

			auto jsonChannelsIt = extendedCodec.find("channels");
			if (jsonChannelsIt != extendedCodec.end() && jsonChannelsIt->is_number())
			{
				auto channels     = *jsonChannelsIt;
				codec["channels"] = channels;
			}

			rtpCapabilities["codecs"].push_back(codec);

			// Add RTX codec.
			if (extendedCodec["remoteRtxPayloadType"] != nullptr)
			{
				auto mimeType = extendedCodec["kind"].get<std::string>();
				mimeType.append("/rtx");

				/* clang-format off */
				json rtxCodec =
				{
					{ "mimeType",             mimeType                              },
					{ "kind",                 extendedCodec["kind"]                 },
					{ "clockRate",            extendedCodec["clockRate"]            },
					{ "preferredPayloadType", extendedCodec["remoteRtxPayloadType"] },
					{ "rtcpFeedback",         json::array()                         },
					{
						"parameters",
						{
							{ "apt", extendedCodec["remotePayloadType"].get<uint8_t>() }
						}
					}
				};
				/* clang-format on */

				rtpCapabilities["codecs"].push_back(rtxCodec);
			}

			// TODO: In the future, we need to add FEC, CN, etc, codecs.
		}

		for (auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
		{
			auto jsonDirectionIt = extendedExtension.find("direction");
			std::string direction;

			if (jsonDirectionIt != extendedExtension.end() && jsonDirectionIt->is_string())
				direction = jsonDirectionIt->get<std::string>();
			else
				direction = "sendrecv";

			// Ignore RTP extensions not valid for receiving.
			if (direction != "sendrecv" && direction != "recvonly")
				continue;

			/* clang-format off */
			json ext =
			{
				{ "kind",        extendedExtension["kind"]   },
				{ "uri",         extendedExtension["uri"]    },
				{ "preferredId", extendedExtension["recvId"] }
			};
			/* clang-format on */

			rtpCapabilities["headerExtensions"].push_back(ext);
		}

		rtpCapabilities["fecMechanisms"] = extendedRtpCapabilities["fecMechanisms"];

		return rtpCapabilities;
	};

	/**
	 * Generate RTP parameters of the given kind for sending media.
	 * Just the first media codec per kind is considered.
	 * NOTE: mid, encodings and rtcp fields are left empty.
	 */
	json getSendingRtpParameters(const std::string& kind, const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

		/* clang-format off */
		json rtpParameters =
		{
			{ "mid",              nullptr        },
			{ "codecs",           json::array()  },
			{ "headerExtensions", json::array()  },
			{ "encodings",        json::array()  },
			{ "rtcp",             json::object() }
		};
		/* clang-format on */

		for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
		{
			if (kind != extendedCodec["kind"].get<std::string>())
				continue;

			/* clang-format off */
			json codec =
			{
				{ "mimeType",             extendedCodec["mimeType"]         },
				{ "kind",                 extendedCodec["kind"]             },
				{ "clockRate",            extendedCodec["clockRate"]        },
				{ "payloadType",          extendedCodec["localPayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]     },
				{ "parameters",           extendedCodec["localParameters"]  }
			};
			/* clang-format on */

			auto jsonChannelsIt = extendedCodec.find("channels");
			if (jsonChannelsIt != extendedCodec.end() && jsonChannelsIt->is_number())
			{
				auto channels     = *jsonChannelsIt;
				codec["channels"] = channels;
			}

			rtpParameters["codecs"].push_back(codec);

			// Add RTX codec.
			if (extendedCodec["localRtxPayloadType"] != nullptr)
			{
				auto mimeType = extendedCodec["kind"].get<std::string>();
				mimeType.append("/rtx");

				/* clang-format off */
				json rtxCodec =
				{
					{ "mimeType",     mimeType                            },
					{ "clockRate",    extendedCodec["clockRate"]          },
					{ "payloadType",  extendedCodec["localRtxPayloadType"] },
					{ "rtcpFeedback", json::array()                       },
					{
						"parameters",
						{
							{ "apt", extendedCodec["localPayloadType"].get<uint8_t>() }
						}
					}
				};
				/* clang-format on */

				rtpParameters["codecs"].push_back(rtxCodec);
			}

			// NOTE: We assume a single media codec plus an optional RTX codec.
			// TODO: In the future, we need to add FEC, CN, etc, codecs.
			break;
		}

		for (auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
		{
			if (kind != extendedExtension["kind"].get<std::string>())
				continue;

			auto jsonDirectionIt = extendedExtension.find("direction");
			std::string direction;

			if (jsonDirectionIt != extendedExtension.end() && jsonDirectionIt->is_string())
				direction = jsonDirectionIt->get<std::string>();
			else
				direction = "sendrecv";

			// Ignore RTP extensions not valid for sending.
			if (direction != "sendrecv" && direction != "sendonly")
				continue;

			/* clang-format off */
			json ext =
			{
				{ "uri", extendedExtension["uri"]    },
				{ "id",  extendedExtension["recvId"] }
			};
			/* clang-format on */

			rtpParameters["headerExtensions"].push_back(ext);
		}

		return rtpParameters;
	};

	/**
	 * Generate RTP parameters of the given kind for sending media.
	 */
	json getSendingRemoteRtpParameters(const std::string& kind, const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

		/* clang-format off */
		json rtpParameters =
		{
			{ "mid",              nullptr        },
			{ "codecs",           json::array()  },
			{ "headerExtensions", json::array()  },
			{ "encodings",        json::array()  },
			{ "rtcp",             json::object() }
		};
		/* clang-format on */

		for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
		{
			if (kind != extendedCodec["kind"].get<std::string>())
				continue;

			/* clang-format off */
			json codec =
			{
				{ "mimeType",             extendedCodec["mimeType"]         },
				{ "kind",                 extendedCodec["kind"]             },
				{ "clockRate",            extendedCodec["clockRate"]        },
				{ "payloadType",          extendedCodec["localPayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]     },
				{ "parameters",           extendedCodec["remoteParameters"] }
			};
			/* clang-format on */

			auto jsonChannelsIt = extendedCodec.find("channels");
			if (jsonChannelsIt != extendedCodec.end() && jsonChannelsIt->is_number())
			{
				auto channels     = *jsonChannelsIt;
				codec["channels"] = channels;
			}

			rtpParameters["codecs"].push_back(codec);

			// Add RTX codec.
			if (extendedCodec["localRtxPayloadType"] != nullptr)
			{
				auto mimeType = extendedCodec["kind"].get<std::string>();
				mimeType.append("/rtx");

				/* clang-format off */
				json rtxCodec =
				{
					{ "mimeType",     mimeType                            },
					{ "clockRate",    extendedCodec["clockRate"]          },
					{ "payloadType",  extendedCodec["localRtxPayloadType"] },
					{ "rtcpFeedback", json::array()                       },
					{
						"parameters",
						{
							{ "apt", extendedCodec["localPayloadType"].get<uint8_t>() }
						}
					}
				};
				/* clang-format on */

				rtpParameters["codecs"].push_back(rtxCodec);
			}

			// NOTE: We assume a single media codec plus an optional RTX codec.
			// TODO: In the future, we need to add FEC, CN, etc, codecs.
			break;
		}

		for (auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
		{
			if (kind != extendedExtension["kind"].get<std::string>())
				continue;

			auto jsonDirectionIt = extendedExtension.find("direction");
			std::string direction;

			if (jsonDirectionIt != extendedExtension.end() && jsonDirectionIt->is_string())
				direction = jsonDirectionIt->get<std::string>();
			else
				direction = "sendrecv";

			// Ignore RTP extensions not valid for sending.
			if (direction != "sendrecv" && direction != "sendonly")
				continue;

			/* clang-format off */
			json ext =
			{
				{ "uri", extendedExtension["uri"]    },
				{ "id",  extendedExtension["recvId"] }
			};
			/* clang-format on */

			rtpParameters["headerExtensions"].push_back(ext);
		}

		// Reduce codecs' RTCP feedback. Use Transport-CC if available, REMB otherwise.
		auto jsonHeaderExtensionIt = std::find_if(
		  rtpParameters["headerExtensions"].begin(),
		  rtpParameters["headerExtensions"].end(),
		  [](json& ext) {
			  return ext["uri"].get<std::string>() ==
			         "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
		  });

		if (jsonHeaderExtensionIt != rtpParameters["headerExtensions"].end())
		{
			for (auto& codec : rtpParameters["codecs"])
			{
				if (codec.find("rtcpFeedback") == codec.end())
					break;

				auto jsonRtcpFeedbackIt = codec["rtcpFeedback"].begin();
				for (; jsonRtcpFeedbackIt != codec["rtcpFeedback"].end();)
				{
					auto& rtcpFeedback = *jsonRtcpFeedbackIt;

					if (rtcpFeedback["type"].get<std::string>() == "goog-remb")
					{
						jsonRtcpFeedbackIt = codec["rtcpFeedback"].erase(jsonRtcpFeedbackIt);
					}
					else
					{
						++jsonRtcpFeedbackIt;
					}
				}
			}

			return rtpParameters;
		}

		jsonHeaderExtensionIt = std::find_if(
		  rtpParameters["headerExtensions"].begin(),
		  rtpParameters["headerExtensions"].end(),
		  [](json& ext) {
			  return ext["uri"].get<std::string>() ==
			         "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
		  });

		if (jsonHeaderExtensionIt != rtpParameters["headerExtensions"].end())
		{
			for (auto& codec : rtpParameters["codecs"])
			{
				if (codec.find("rtcpFeedback") == codec.end())
					break;

				auto jsonRtcpFeedbackIt = codec["rtcpFeedback"].begin();
				for (; jsonRtcpFeedbackIt != codec["rtcpFeedback"].end();)
				{
					auto& rtcpFeedback = *jsonRtcpFeedbackIt;

					if (rtcpFeedback["type"].get<std::string>() == "transport-cc")
					{
						jsonRtcpFeedbackIt = codec["rtcpFeedback"].erase(jsonRtcpFeedbackIt);
					}
					else
					{
						++jsonRtcpFeedbackIt;
					}
				}
			}

			return rtpParameters;
		}

		for (auto& codec : rtpParameters["codecs"])
		{
			if (codec.find("rtcpFeedback") == codec.end())
				break;

			auto jsonRtcpFeedbackIt = codec["rtcpFeedback"].begin();
			for (; jsonRtcpFeedbackIt != codec["rtcpFeedback"].end();)
			{
				auto& rtcpFeedback = *jsonRtcpFeedbackIt;

				/* clang-format off */
				if (
					rtcpFeedback["type"].get<std::string>() == "transport-cc" ||
					rtcpFeedback["type"].get<std::string>() == "goog-remb"
				)
				{
					jsonRtcpFeedbackIt = codec["rtcpFeedback"].erase(jsonRtcpFeedbackIt);
				} else {
					++jsonRtcpFeedbackIt;
				}
				/* clang-format on */
			}
		}

		return rtpParameters;
	};

	/**
	 * Whether media can be sent based on the given RTP capabilities.
	 */
	bool canSend(const std::string& kind, const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

		auto codecs = extendedRtpCapabilities["codecs"];

		auto jsonCodecIt = std::find_if(codecs.begin(), codecs.end(), [&kind](const json& codec) {
			return kind == codec["kind"].get<std::string>();
		});

		return jsonCodecIt != codecs.end();
	}

	/**
	 * Whether the given RTP parameters can be received with the given RTP
	 * capabilities.
	 */
	bool canReceive(const json& rtpParameters, const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

		if (rtpParameters["codecs"].empty())
			return false;

		auto firstMediaCodec = rtpParameters["codecs"][0];

		auto codecs = extendedRtpCapabilities["codecs"];
		auto jsonCodecIt =
		  std::find_if(codecs.begin(), codecs.end(), [&firstMediaCodec](const json& codec) {
			  return codec["remotePayloadType"] == firstMediaCodec["payloadType"];
		  });

		return jsonCodecIt != codecs.end();
	}
} // namespace ortc
} // namespace mediasoupclient
