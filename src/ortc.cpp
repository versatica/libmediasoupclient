#define MSC_CLASS "ortc"
// #define MSC_LOG_DEV

#include "ortc.hpp"
#include "Logger.hpp"
#include <algorithm> // std::find_if
#include <string>

using json = nlohmann::json;

namespace mediasoupclient
{
namespace ortc
{
	static uint8_t getH264PacketizationMode(const json& codec)
	{
		MSC_TRACE();

		auto it = codec.find("parameters");
		if (it == codec.end())
			return 0;

		auto parameters = *it;
		it              = parameters.find("packetization-mode");
		if (it == parameters.end() || !it->is_number())
			return 0;

		return it->get<uint8_t>();
	}

	static bool matchCapCodecs(const json& aCodec, const json& bCodec)
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
		MSC_TRACE();

		if (aExt["kind"] != bExt["kind"])
			return false;

		return aExt["uri"] == bExt["uri"];
	}

	static json reduceRtcpFeedback(const json& codecA, const json& codecB)
	{
		MSC_TRACE();

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
	json getExtendedRtpCapabilities(const json& localCaps, const json& remoteCaps)
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
					{ "name",                 localCodec["name"]                          },
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

				auto it = localCodec.find("channels");
				if (it != localCodec.end())
				{
					auto channels = (*it).get<uint8_t>();
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
			auto localCodecs = localCaps["codecs"];
			auto it          = std::find_if(
        localCodecs.begin(), localCodecs.end(), [&extendedCodec](const json& localCodec) {
          return "rtx" == localCodec["name"].get<std::string>() &&
                 localCodec["parameters"]["apt"] == extendedCodec["localPayloadType"];
        });

			if (it == localCodecs.end())
				continue;

			auto matchingLocalRtxCodec = *it;

			auto remoteCodecs = remoteCaps["codecs"];
			it                = std::find_if(
        remoteCodecs.begin(), remoteCodecs.end(), [&extendedCodec](const json& remoteCodec) {
          return "rtx" == remoteCodec["name"].get<std::string>() &&
                 remoteCodec["parameters"]["apt"] == extendedCodec["remotePayloadType"];
        });

			if (it == remoteCodecs.end())
				continue;

			auto matchingRemoteRtxCodec = *it;

			extendedCodec["localRtxPayloadType"]  = matchingLocalRtxCodec["preferredPayloadType"];
			extendedCodec["remoteRtxPayloadType"] = matchingRemoteRtxCodec["preferredPayloadType"];
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
				{ "name",                 extendedCodec["name"]              },
				{ "mimeType",             extendedCodec["mimeType"]          },
				{ "kind",                 extendedCodec["kind"]              },
				{ "clockRate",            extendedCodec["clockRate"]         },
				{ "preferredPayloadType", extendedCodec["remotePayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]      },
				{ "parameters",           extendedCodec["localParameters"]   }
			};
			/* clang-format on */

			auto it = extendedCodec.find("channels");
			if (it != extendedCodec.end())
			{
				auto channels     = *it;
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
					{ "name",                 "rtx"                                 },
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
				{ "name",                 extendedCodec["name"]             },
				{ "mimeType",             extendedCodec["mimeType"]         },
				{ "kind",                 extendedCodec["kind"]             },
				{ "clockRate",            extendedCodec["clockRate"]        },
				{ "payloadType",          extendedCodec["localPayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]     },
				{ "parameters",           extendedCodec["localParameters"]  }
			};
			/* clang-format on */

			auto it = extendedCodec.find("channels");
			if (it != extendedCodec.end())
			{
				auto channels     = *it;
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
					{ "name",         "rtx"                               },
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
				{ "name",                 extendedCodec["name"]            },
				{ "mimeType",             extendedCodec["mimeType"]         },
				{ "kind",                 extendedCodec["kind"]             },
				{ "clockRate",            extendedCodec["clockRate"]        },
				{ "payloadType",          extendedCodec["localPayloadType"] },
				{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]     },
				{ "parameters",           extendedCodec["remoteParameters"] }
			};
			/* clang-format on */

			auto it = extendedCodec.find("channels");
			if (it != extendedCodec.end())
			{
				auto channels     = *it;
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
					{ "name",         "rtx"                               },
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
	 * Whether media can be sent based on the given RTP capabilities.
	 */
	bool canSend(const std::string& kind, const json& extendedRtpCapabilities)
	{
		MSC_TRACE();

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
		MSC_TRACE();

		if (rtpParameters["codecs"].empty())
			return false;

		auto firstMediaCodec = rtpParameters["codecs"][0];

		auto codecs = extendedRtpCapabilities["codecs"];
		auto it     = std::find_if(codecs.begin(), codecs.end(), [&firstMediaCodec](const json& codec) {
      return codec["remotePayloadType"] == firstMediaCodec["payloadType"];
    });

		return it != codecs.end();
	}
} // namespace ortc
} // namespace mediasoupclient
