#define MSC_CLASS "ortc"
// #define MSC_LOG_DEV

#include "ortc.hpp"
#include "Logger.hpp"
#include <algorithm> // std::find_if
#include <string>

namespace ortc
{
	static std::string getH264PacketizationMode(const json& codec)
	{
		auto it = codec.find("parameters");
		if (it == codec.end())
			return "0";

		auto parameters = *it;
		it              = parameters.find("packetization-mode");
		if (it == parameters.end())
			return "0";

		return (*it).get<std::string>();
	}

	static json::const_iterator findMatchingRtxCodec(const json& codecs, const json& extendedCodec)
	{
		return std::find_if(codecs.begin(), codecs.end(), [&extendedCodec](const json& codec) {
			if ("rtx" != codec["name"].get<std::string>())
				return false;

			auto it = codec.find("parameters");
			if (it == codec.end())
				return false;

			auto parameters = *it;
			it              = parameters.find("apt");
			if (it == parameters.end())
				return false;

			auto apt             = (*it).get<std::string>();
			auto recvPayloadType = std::to_string(extendedCodec["recvPayloadType"].get<uint8_t>());
			return apt == recvPayloadType;
		});
	}

	static bool matchCapCodecs(const json& aCodec, const json& bCodec)
	{
		auto aMimeType = aCodec["mimeType"].get<std::string>();
		auto bMimeType = bCodec["mimeType"].get<std::string>();

		std::transform(aMimeType.begin(), aMimeType.end(), aMimeType.begin(), ::tolower);
		std::transform(bMimeType.begin(), bMimeType.end(), bMimeType.begin(), ::tolower);

		if (aMimeType != bMimeType)
			return false;

		if (aCodec["clockRate"] != bCodec["clockRate"])
			return false;

		auto itA = aCodec.find("channels");
		auto itB = bCodec.find("channels");

		if (itA == aCodec.end() && itB != bCodec.end())
			return false;

		if (itA != aCodec.end() && itB == bCodec.end())
			return false;

		if (itA != aCodec.end() && aCodec["channels"] != bCodec["channels"])
			return false;

		// Match H264 parameters.
		if (aMimeType == "video/h264")
		{
			auto aPacketizationMode = getH264PacketizationMode(aCodec);
			auto bPacketizationMode = getH264PacketizationMode(bCodec);

			if (aPacketizationMode != bPacketizationMode)
				return false;
		}

		return true;
	}

	static bool matchHeaderExtensions(const json& aExt, const json& bExt)
	{
		if (aExt["kind"] != bExt["kind"])
			return false;

		return aExt["uri"] == bExt["uri"];
	}

	static json reduceRtcpFeedback(const json& codecA, const json& codecB)
	{
		auto reducedRtcpFeedback = json::array();

		auto aFbs = codecA["rtcpFeedback"];
		auto bFbs = codecB["rtcpFeedback"];

		for (auto& aFb : aFbs)
		{
			auto it = std::find_if(bFbs.begin(), bFbs.end(), [&aFb](const json& bFb) {
				if (aFb["type"] != bFb["type"])
					return false;

				auto itA = aFb.find("parameter");
				auto itB = bFb.find("parameter");

				if (itA == aFb.end() && itB != bFb.end())
					return false;

				if (itA != aFb.end() && itB == bFb.end())
					return false;

				if (itA == aFb.end())
					return true;

				return (*itA) == (*itB);
			});

			if (it != bFbs.end())
				reducedRtcpFeedback.push_back(*it);
		}

		return reducedRtcpFeedback;
	}

	/**
	 * Generate extended RTP capabilities for sending and receiving.
	 */
	json getExtendedCapabilities(const json& localCaps, const json& remoteCaps)
	{
		/* clang-format off */
		json extendedCaps =
		{
			{ "codecs",           json::array() },
			{ "headerExtensions", json::array() },
			{ "fecMechanisms",    json::array() }
		};
		/* clang-format on */

		// Match media codecs and keep the order preferred by remoteCaps.
		for (auto& remoteCodec : remoteCaps["codecs"])
		{
			// TODO: Ignore pseudo-codecs and feature codecs.
			if ("rtx" == remoteCodec["name"].get<std::string>())
				continue;

			auto localCodecs = localCaps["codecs"];

			auto it =
			  std::find_if(localCodecs.begin(), localCodecs.end(), [&remoteCodec](const json& localCodec) {
				  return matchCapCodecs(localCodec, remoteCodec);
			  });

			if (it != localCodecs.end())
			{
				auto localCodec = *it;

				/* clang-format off */
				json extendedCodec =
				{
					{ "name",               remoteCodec["name"]                         },
					{ "mimeType",           remoteCodec["mimeType"]                     },
					{ "kind",               remoteCodec["kind"]                         },
					{ "clockRate",          remoteCodec["clockRate"]                    },
					{ "sendPayloadType",    localCodec["preferredPayloadType"]          },
					{ "sendRtxPayloadType", nullptr                                     },
					{ "recvPayloadType",    remoteCodec["preferredPayloadType"]         },
					{ "recvRtxPayloadType", nullptr                                     },
					{ "rtcpFeedback",       reduceRtcpFeedback(localCodec, remoteCodec) },
					{ "parameters",         localCodec["parameters"]                    }
				};
				/* clang-format on */

				auto it = remoteCodec.find("channels");
				if (it != remoteCodec.end())
				{
					auto channels = (*it).get<uint8_t>();
					if (channels > 0)
						extendedCodec["channels"] = channels;
				}

				extendedCaps["codecs"].push_back(extendedCodec);
			}
		}

		// Match RTX codecs.
		json& extendedCodecs = extendedCaps["codecs"];
		for (json& extendedCodec : extendedCodecs)
		{
			auto localCodecs = localCaps["codecs"];
			auto it          = findMatchingRtxCodec(localCodecs, extendedCodec);
			if (it == localCodecs.end())
				continue;

			auto matchingLocalRtxCodec = *it;

			auto remoteCodecs = remoteCaps["codecs"];
			it                = findMatchingRtxCodec(remoteCodecs, extendedCodec);
			if (it == remoteCodecs.end())
				continue;

			auto matchingRemoteRtxCodec = *it;

			extendedCodec["sendRtxPayloadType"] = matchingLocalRtxCodec["preferredPayloadType"];
			extendedCodec["recvRtxPayloadType"] = matchingRemoteRtxCodec["preferredPayloadType"];
		}

		// Match header extensions.
		auto remoteExts = remoteCaps["headerExtensions"];
		for (auto& remoteExt : remoteExts)
		{
			auto localExts = localCaps["headerExtensions"];

			auto it = std::find_if(localExts.begin(), localExts.end(), [&remoteExt](const json& localExt) {
				return matchHeaderExtensions(localExt, remoteExt);
			});

			if (it == localExts.end())
				continue;

			auto matchingLocalExt = *it;

			/* clang-format off */
			json extendedExt =
			{
				{ "kind",   remoteExt["kind"]               },
				{ "uri",    remoteExt["uri"]                },
				{ "sendId", matchingLocalExt["preferredId"] },
				{ "recvId", remoteExt["preferredId"]        }
			};
			/* clang-format on */

			extendedCaps["headerExtensions"].push_back(extendedExt);
		}

		return extendedCaps;
	}

	/**
	 * Generate RTP capabilities for receiving media based on the given extended
	 * RTP capabilities.
	 */
	json getRecvRtpCapabilities(const json& extendedRtpCapabilities)
	{
		/* clang-format off */
		json caps =
		{
			{ "codecs",           json::array() },
			{ "headerExtensions", json::array() },
			{ "fecMechanisms",    json::array() }
		};
		/* clang-format on */

		for (auto& capCodec : extendedRtpCapabilities["codecs"])
		{
			/* clang-format off */
			json codec =
			{
				{ "name",                 capCodec["name"] },
				{ "mimeType",             capCodec["mimeType"] },
				{ "kind",                 capCodec["kind"] },
				{ "clockRate",            capCodec["clockRate"] },
				{ "preferredPayloadType", capCodec["recvPayloadType"] },
				{ "rtcpFeedback",         capCodec["rtcpFeedback"] },
				{ "parameters",           capCodec["parameters"] }
			};
			/* clang-format on */

			auto it = capCodec.find("channels");
			if (it != capCodec.end())
			{
				auto channels     = *it;
				codec["channels"] = channels;
			}

			caps["codecs"].push_back(codec);

			// Add RTX codec.
			if (capCodec["recvRtxPayloadType"] != nullptr)
			{
				auto mimeType = capCodec["kind"].get<std::string>();
				mimeType.append("/rtx");

				/* clang-format off */
				json rtxCapCodec =
				{
					{ "name",                 "rtx"                          },
					{ "mimeType",             mimeType                       },
					{ "kind",                 capCodec["kind"]               },
					{ "clockRate",            capCodec["clockRate"]          },
					{ "preferredPayloadType", capCodec["recvRtxPayloadType"] },
					{ "rtcpFeedback",         json::array()                  },
					{
						"parameters",
						{
							{ "apt", std::to_string(capCodec["recvPayloadType"].get<uint8_t>()) }
						}
					}
				};
				/* clang-format on */

				caps["codecs"].push_back(rtxCapCodec);
			}

			// TODO: In the future, we need to add FEC, CN, etc, codecs.
		}

		for (auto& capExt : extendedRtpCapabilities["headerExtensions"])
		{
			/* clang-format off */
			json ext =
			{
				{ "kind",        capExt["kind"]   },
				{ "uri",         capExt["uri"]    },
				{ "preferredId", capExt["recvId"] }
			};
			/* clang-format on */

			caps["headerExtensions"].push_back(ext);
		}

		caps["fecMechanisms"] = extendedRtpCapabilities["fecMechanisms"];

		return caps;
	};

	/**
	 * Generate RTP parameters of the given kind for sending media.
	 * Just the first media codec per kind is considered.
	 * NOTE: mid, encodings and rtcp fields are left empty.
	 */
	json getSendingRtpParameters(const std::string& kind, const json& extendedRtpCapabilities)
	{
		/* clang-format off */
		json params =
		{
			{ "mid",              nullptr       },
			{ "codecs",           json::array() },
			{ "headerExtensions", json::array() },
			{ "encodings",        json::array() },
			{ "rtcp",             json::array() }
		};
		/* clang-format on */

		for (auto& capCodec : extendedRtpCapabilities["codecs"])
		{
			if (kind != capCodec["kind"].get<std::string>())
				continue;

			/* clang-format off */
			json codec =
			{
				{ "name",                 capCodec["name"] },
				{ "mimeType",             capCodec["mimeType"] },
				{ "kind",                 capCodec["kind"] },
				{ "clockRate",            capCodec["clockRate"] },
				{ "payloadType",          capCodec["recvPayloadType"] },
				{ "rtcpFeedback",         capCodec["rtcpFeedback"] },
				{ "parameters",           capCodec["parameters"] }
			};
			/* clang-format on */

			auto it = capCodec.find("channels");
			if (it != capCodec.end())
			{
				auto channels     = *it;
				codec["channels"] = channels;
			}

			params["codecs"].push_back(codec);

			// Add RTX codec.
			if (capCodec["sendRtxPayloadType"] != nullptr)
			{
				auto mimeType = capCodec["kind"].get<std::string>();
				mimeType.append("/rtx");

				/* clang-format off */
				json rtxCapCodec =
				{
					{ "name",         "rtx"                          },
					{ "mimeType",     mimeType                       },
					{ "clockRate",    capCodec["clockRate"]          },
					{ "payloadType",  capCodec["sendRtxPayloadType"] },
					{ "rtcpFeedback", json::array()                  },
					{
						"parameters",
						{
							{ "apt", std::to_string(capCodec["sendPayloadType"].get<uint8_t>()) }
						}
					}
				};
				/* clang-format on */

				params["codecs"].push_back(rtxCapCodec);
			}

			// NOTE: We assume a single media codec plus an optional RTX codec.
			// TODO: In the future, we need to add FEC, CN, etc, codecs.
			break;
		}

		for (auto& capExt : extendedRtpCapabilities["headerExtensions"])
		{
			if (kind != capExt["kind"].get<std::string>())
				continue;

			/* clang-format off */
			json ext =
			{
				{ "uri", capExt["uri"]    },
				{ "id",  capExt["recvId"] }
			};
			/* clang-format on */

			params["headerExtensions"].push_back(ext);
		}

		return params;
	};

	/**
	 * Whether media can be sent based on the given RTP capabilities.
	 */
	bool canSend(const std::string& kind, const json& extendedRtpCapabilities)
	{
		auto codecs = extendedRtpCapabilities["codecs"];

		auto it = std::find_if(codecs.begin(), codecs.end(), [&kind](const json& codec) {
			return kind == codec["kind"].get<std::string>();
		});

		return it != codecs.end();
	}

	/**
	 * Whether the given RTP parameters can be received with the given RTP
	 * capabilities.
	 */
	bool canReceive(const json& rtpParameters, const json& extendedRtpCapabilities)
	{
		if (rtpParameters["codecs"].empty())
			return false;

		auto firstMediaCodec = rtpParameters["codecs"][0];

		auto codecs = extendedRtpCapabilities["codecs"];
		auto it     = std::find_if(codecs.begin(), codecs.end(), [&firstMediaCodec](const json& codec) {
      return codec["recvPayloadType"] == firstMediaCodec["payloadType"];
    });

		return it != codecs.end();
	}
} // namespace ortc
