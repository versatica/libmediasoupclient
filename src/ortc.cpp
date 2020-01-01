#define MSC_CLASS "ortc"

#include "ortc.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include <media/base/h264_profile_level_id.h>
#include <algorithm> // std::find_if
#include <regex>
#include <string>

using json = nlohmann::json;

static constexpr uint32_t ProbatorSsrc{ 1234u };

// Static functions declaration.
static bool isRtxCodec(const json& codec);
static bool matchCodecs(json& aCodec, const json& bCodec, bool strict = false, bool modify = false);
static bool matchHeaderExtensions(const json& aExt, const json& bExt);
static json reduceRtcpFeedback(const json& codecA, const json& codecB);
static uint8_t getH264PacketizationMode(const json& codec);
static uint8_t getH264LevelAssimetryAllowed(const json& codec);
static std::string getH264ProfileLevelId(const json& codec);

namespace mediasoupclient
{
	namespace ortc
	{
		/**
		 * Validates RTCRtpCapabilities. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpCapabilities(json& caps)
		{
			MSC_TRACE();

			if (!caps.is_object())
				MSC_THROW_TYPE_ERROR("caps is not an object");

			auto codecsIt           = caps.find("codecs");
			auto headerExtensionsIt = caps.find("headerExtensions");

			// codecs is optional. If unset, fill with an empty array.
			if (codecsIt != caps.end() && !codecsIt->is_array())
			{
				MSC_THROW_TYPE_ERROR("caps.codecs is not an array");
			}
			else if (codecsIt == caps.end())
			{
				caps["codecs"] = json::array();
				codecsIt       = caps.find("codecs");
			}

			for (auto& codec : *codecsIt)
			{
				validateRtpCodecCapability(codec);
			}

			// headerExtensions is optional. If unset, fill with an empty array.
			if (headerExtensionsIt != caps.end() && !headerExtensionsIt->is_array())
			{
				MSC_THROW_TYPE_ERROR("caps.headerExtensions is not an array");
			}
			else if (headerExtensionsIt == caps.end())
			{
				caps["headerExtensions"] = json::array();
				headerExtensionsIt       = caps.find("headerExtensions");
			}

			for (auto& ext : *headerExtensionsIt)
			{
				validateRtpHeaderExtension(ext);
			}
		}

		/**
		 * Validates RtpCodecCapability. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpCodecCapability(json& codec)
		{
			MSC_TRACE();

			static const std::regex MimeTypeRegex(
			  "^(audio|video)/(.+)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			if (!codec.is_object())
				MSC_THROW_TYPE_ERROR("codec is not an object");

			auto mimeTypeIt             = codec.find("mimeType");
			auto preferredPayloadTypeIt = codec.find("preferredPayloadType");
			auto clockRateIt            = codec.find("clockRate");
			auto channelsIt             = codec.find("channels");
			auto parametersIt           = codec.find("parameters");
			auto rtcpFeedbackIt         = codec.find("rtcpFeedback");

			// mimeType is mandatory.
			if (mimeTypeIt == codec.end() || !mimeTypeIt->is_string())
				MSC_THROW_TYPE_ERROR("missing codec.mimeType");

			std::smatch mimeTypeMatch;
			std::regex_match(mimeTypeIt->get<std::string>(), mimeTypeMatch, MimeTypeRegex);

			if (mimeTypeMatch.empty())
				MSC_THROW_TYPE_ERROR("invalid codec.mimeType");

			// Just override kind with media component of mimeType.
			codec["kind"] = mimeTypeMatch[1].str();

			// preferredPayloadType is optional.
			if (preferredPayloadTypeIt != codec.end() && !preferredPayloadTypeIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("invalid codec.preferredPayloadType");

			// clockRate is mandatory.
			if (clockRateIt == codec.end() || !clockRateIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing codec.clockRate");

			// channels is optional. If unset, set it to 1.
			if (channelsIt == codec.end() || !channelsIt->is_number_integer())
				codec["channels"] = 1;

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == codec.end() || !parametersIt->is_object())
			{
				codec["parameters"] = json::object();
				parametersIt        = codec.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				auto& key   = it.key();
				auto& value = it.value();

				if (!value.is_string() && !value.is_number())
					MSC_THROW_TYPE_ERROR("invalid codec parameter");

				// Specific parameters validation.
				if (key == "apt")
				{
					if (!value.is_number_integer())
						MSC_THROW_TYPE_ERROR("invalid codec apt parameter");
				}
			}

			// rtcpFeedback is optional. If unset, set it to an empty array.
			if (rtcpFeedbackIt == codec.end() || !rtcpFeedbackIt->is_array())
			{
				codec["rtcpFeedback"] = json::array();
				rtcpFeedbackIt        = codec.find("rtcpFeedback");
			}

			for (auto& fb : *rtcpFeedbackIt)
			{
				validateRtcpFeedback(fb);
			}
		}

		/**
		 * Validates RtcpFeedback. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtcpFeedback(json& fb)
		{
			MSC_TRACE();

			if (!fb.is_object())
				MSC_THROW_TYPE_ERROR("fb is not an object");

			auto typeIt      = fb.find("type");
			auto parameterIt = fb.find("parameter");

			// type is mandatory.
			if (typeIt == fb.end() || !typeIt->is_string())
				MSC_THROW_TYPE_ERROR("missing fb.type");

			// parameter is optional. If unset set it to an empty string.
			if (parameterIt == fb.end() || !parameterIt->is_string())
				fb["parameter"] = "";
		}

		/**
		 * Validates RtpHeaderExtension. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpHeaderExtension(json& ext)
		{
			MSC_TRACE();

			if (!ext.is_object())
				MSC_THROW_TYPE_ERROR("ext is not an object");

			auto kindIt             = ext.find("kind");
			auto uriIt              = ext.find("uri");
			auto preferredIdIt      = ext.find("preferredId");
			auto preferredEncryptIt = ext.find("preferredEncrypt");
			auto directionIt        = ext.find("direction");

			// kind is optional. If unset set it to an empty string.
			if (kindIt == ext.end() || !kindIt->is_string())
				ext["kind"] = "";

			kindIt           = ext.find("kind");
			std::string kind = kindIt->get<std::string>();

			if (kind != "" && kind != "audio" && kind != "video")
				MSC_THROW_TYPE_ERROR("invalid ext.kind");

			// uri is mandatory.
			if (uriIt == ext.end() || !uriIt->is_string() || uriIt->get<std::string>().empty())
				MSC_THROW_TYPE_ERROR("missing ext.uri");

			// preferredId is mandatory.
			if (preferredIdIt == ext.end() || !preferredIdIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing ext.preferredId");

			// preferredEncrypt is optional. If unset set it to false.
			if (preferredEncryptIt != ext.end() && !preferredEncryptIt->is_boolean())
				MSC_THROW_TYPE_ERROR("invalid ext.preferredEncrypt");
			else if (preferredEncryptIt == ext.end())
				ext["preferredEncrypt"] = false;

			// direction is optional. If unset set it to sendrecv.
			if (directionIt != ext.end() && !directionIt->is_string())
				MSC_THROW_TYPE_ERROR("invalid ext.direction");
			else if (directionIt == ext.end())
				ext["direction"] = "sendrecv";
		}

		/**
		 * Validates RtpParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpParameters(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto midIt              = params.find("mid");
			auto codecsIt           = params.find("codecs");
			auto headerExtensionsIt = params.find("headerExtensions");
			auto encodingsIt        = params.find("encodings");
			auto rtcpIt             = params.find("rtcp");

			// mid is optional.
			if (midIt != params.end() && (!midIt->is_string() || midIt->get<std::string>().empty()))
				MSC_THROW_TYPE_ERROR("params.mid is not a string");

			// codecs is mandatory.
			if (codecsIt == params.end() || !codecsIt->is_array())
				MSC_THROW_TYPE_ERROR("missing params.codecs");

			for (auto& codec : *codecsIt)
			{
				validateRtpCodecParameters(codec);
			}

			// headerExtensions is optional. If unset, fill with an empty array.
			if (headerExtensionsIt != params.end() && !headerExtensionsIt->is_array())
			{
				MSC_THROW_TYPE_ERROR("params.headerExtensions is not an array");
			}
			else if (headerExtensionsIt == params.end())
			{
				params["headerExtensions"] = json::array();
				headerExtensionsIt         = params.find("headerExtensions");
			}

			for (auto& ext : *headerExtensionsIt)
			{
				validateRtpHeaderExtensionParameters(ext);
			}

			// encodings is optional. If unset, fill with an empty array.
			if (encodingsIt != params.end() && !encodingsIt->is_array())
			{
				MSC_THROW_TYPE_ERROR("params.encodings is not an array");
			}
			else if (encodingsIt == params.end())
			{
				params["encodings"] = json::array();
				encodingsIt         = params.find("encodings");
			}

			for (auto& encoding : *encodingsIt)
			{
				validateRtpEncodingParameters(encoding);
			}

			// rtcp is optional. If unset, fill with an empty object.
			if (rtcpIt != params.end() && !rtcpIt->is_object())
			{
				MSC_THROW_TYPE_ERROR("params.rtcp is not an object");
			}
			else if (rtcpIt == params.end())
			{
				params["rtcp"] = json::object();
				rtcpIt         = params.find("rtcp");
			}

			validateRtcpParameters(*rtcpIt);
		}

		/**
		 * Validates RtpCodecParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpCodecParameters(json& codec)
		{
			MSC_TRACE();

			static const std::regex MimeTypeRegex(
			  "^(audio|video)/(.+)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			if (!codec.is_object())
				MSC_THROW_TYPE_ERROR("codec is not an object");

			auto mimeTypeIt     = codec.find("mimeType");
			auto payloadTypeIt  = codec.find("payloadType");
			auto clockRateIt    = codec.find("clockRate");
			auto channelsIt     = codec.find("channels");
			auto parametersIt   = codec.find("parameters");
			auto rtcpFeedbackIt = codec.find("rtcpFeedback");

			// mimeType is mandatory.
			if (mimeTypeIt == codec.end() || !mimeTypeIt->is_string())
				MSC_THROW_TYPE_ERROR("missing codec.mimeType");

			std::smatch mimeTypeMatch;
			std::regex_match(mimeTypeIt->get<std::string>(), mimeTypeMatch, MimeTypeRegex);

			if (mimeTypeMatch.empty())
				MSC_THROW_TYPE_ERROR("invalid codec.mimeType");

			// payloadType is mandatory.
			if (payloadTypeIt == codec.end() || !payloadTypeIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing codec.payloadType");

			// clockRate is mandatory.
			if (clockRateIt == codec.end() || !clockRateIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing codec.clockRate");

			// channels is optional. If unset, set it to 1.
			if (channelsIt == codec.end() || !channelsIt->is_number_integer())
				codec["channels"] = 1;

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == codec.end() || !parametersIt->is_object())
			{
				codec["parameters"] = json::object();
				parametersIt        = codec.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				auto& key   = it.key();
				auto& value = it.value();

				if (!value.is_string() && !value.is_number())
					MSC_THROW_TYPE_ERROR("invalid codec parameter");

				// Specific parameters validation.
				if (key == "apt")
				{
					if (!value.is_number_integer())
						MSC_THROW_TYPE_ERROR("invalid codec apt parameter");
				}
			}

			// rtcpFeedback is optional. If unset, set it to an empty array.
			if (rtcpFeedbackIt == codec.end() || !rtcpFeedbackIt->is_array())
			{
				codec["rtcpFeedback"] = json::array();
				rtcpFeedbackIt        = codec.find("rtcpFeedback");
			}

			for (auto& fb : *rtcpFeedbackIt)
			{
				validateRtcpFeedback(fb);
			}
		}

		/**
		 * Validates RtpHeaderExtensionParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpHeaderExtensionParameters(json& ext)
		{
			MSC_TRACE();

			if (!ext.is_object())
				MSC_THROW_TYPE_ERROR("ext is not an object");

			auto uriIt        = ext.find("uri");
			auto idIt         = ext.find("id");
			auto encryptIt    = ext.find("encrypt");
			auto parametersIt = ext.find("parameters");

			// uri is mandatory.
			if (uriIt == ext.end() || !uriIt->is_string() || uriIt->get<std::string>().empty())
				MSC_THROW_TYPE_ERROR("missing ext.uri");

			// id is mandatory.
			if (idIt == ext.end() || !idIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing ext.id");

			// encrypt is optional. If unset set it to false.
			if (encryptIt != ext.end() && !encryptIt->is_boolean())
				MSC_THROW_TYPE_ERROR("invalid ext.encrypt");
			else if (encryptIt == ext.end())
				ext["encrypt"] = false;

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == ext.end() || !parametersIt->is_object())
			{
				ext["parameters"] = json::object();
				parametersIt      = ext.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				auto& value = it.value();

				if (!value.is_string() && !value.is_number())
					MSC_THROW_TYPE_ERROR("invalid header extension parameter");
			}
		}

		/**
		 * Validates RtpEncodingParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpEncodingParameters(json& encoding)
		{
			MSC_TRACE();

			if (!encoding.is_object())
				MSC_THROW_TYPE_ERROR("encoding is not an object");

			auto ssrcIt            = encoding.find("ssrc");
			auto ridIt             = encoding.find("rid");
			auto rtxIt             = encoding.find("rtx");
			auto dtxIt             = encoding.find("dtx");
			auto scalabilityModeIt = encoding.find("scalabilityMode");

			// ssrc is optional.
			if (ssrcIt != encoding.end() && !ssrcIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("invalid encoding.ssrc");

			// rid is optional.
			if (ridIt != encoding.end() && (!ridIt->is_string() || ridIt->get<std::string>().empty()))
				MSC_THROW_TYPE_ERROR("invalid encoding.rid");

			// rtx is optional.
			if (rtxIt != encoding.end() && !rtxIt->is_object())
			{
				MSC_THROW_TYPE_ERROR("invalid encoding.rtx");
			}
			else if (rtxIt != encoding.end())
			{
				auto rtxSsrcIt = rtxIt->find("ssrc");

				// RTX ssrc is mandatory if rtx is present.
				if (rtxSsrcIt == rtxIt->end() || !rtxSsrcIt->is_number_integer())
					MSC_THROW_TYPE_ERROR("missing encoding.rtx.ssrc");
			}

			// dtx is optional. If unset set it to false.
			if (dtxIt == encoding.end() || !dtxIt->is_boolean())
				encoding["dtx"] = false;

			// scalabilityMode is optional.
			// clang-format off
			if (
				scalabilityModeIt != encoding.end() &&
				(!scalabilityModeIt->is_string() || scalabilityModeIt->get<std::string>().empty())
			)
			// clang-format on
			{
				MSC_THROW_TYPE_ERROR("invalid encoding.scalabilityMode");
			}
		}

		/**
		 * Validates RtcpParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtcpParameters(json& rtcp)
		{
			MSC_TRACE();

			if (!rtcp.is_object())
				MSC_THROW_TYPE_ERROR("rtcp is not an object");

			auto cnameIt       = rtcp.find("cname");
			auto reducedSizeIt = rtcp.find("reducedSize");

			// cname is optional.
			if (cnameIt != rtcp.end() && (!cnameIt->is_string() || cnameIt->get<std::string>().empty()))
				MSC_THROW_TYPE_ERROR("invalid rtcp.cname");

			// reducedSize is optional. If unset set it to true.
			if (reducedSizeIt == rtcp.end() || !reducedSizeIt->is_boolean())
				rtcp["reducedSize"] = true;
		}

		/**
		 * Generate extended RTP capabilities for sending and receiving.
		 */
		json getExtendedRtpCapabilities(json& localCaps, json& remoteCaps)
		{
			MSC_TRACE();

			// This may throw.
			validateRtpCapabilities(localCaps);
			validateRtpCapabilities(remoteCaps);

			static const std::regex MimeTypeRegex(
			  "^(audio|video)/(.+)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			// clang-format off
			json extendedRtpCapabilities =
			{
				{ "codecs",           json::array() },
				{ "headerExtensions", json::array() },
				{ "fecMechanisms",    json::array() }
			};
			// clang-format on

			// Match media codecs and keep the order preferred by remoteCaps.
			auto remoteCapsCodecsIt = remoteCaps.find("codecs");
			for (auto& remoteCodec : *remoteCapsCodecsIt)
			{
				if (isRtxCodec(remoteCodec))
					continue;

				json& localCodecs = localCaps["codecs"];

				auto matchingLocalCodecIt =
				  std::find_if(localCodecs.begin(), localCodecs.end(), [&remoteCodec](json& localCodec) {
					  return matchCodecs(localCodec, remoteCodec, /*strict*/ true, /*modify*/ true);
				  });

				if (matchingLocalCodecIt == localCodecs.end())
					continue;

				auto& localCodec = *matchingLocalCodecIt;

				// clang-format off
				json extendedCodec =
				{
					{ "mimeType",             localCodec["mimeType"]                      },
					{ "kind",                 localCodec["kind"]                          },
					{ "clockRate",            localCodec["clockRate"]                     },
					{ "channels",             localCodec["channels"]                      },
					{ "localPayloadType",     localCodec["preferredPayloadType"]          },
					{ "localRtxPayloadType",  nullptr                                     },
					{ "remotePayloadType",    remoteCodec["preferredPayloadType"]         },
					{ "remoteRtxPayloadType", nullptr                                     },
					{ "localParameters",      localCodec["parameters"]                    },
					{ "remoteParameters",     remoteCodec["parameters"]                   },
					{ "rtcpFeedback",         reduceRtcpFeedback(localCodec, remoteCodec) }
				};
				// clang-format on

				extendedRtpCapabilities["codecs"].push_back(extendedCodec);
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

				auto& matchingLocalRtxCodec = *jsonLocalCodecIt;
				auto remoteCodecs           = remoteCaps["codecs"];
				auto jsonRemoteCodecIt      = std::find_if(
          remoteCodecs.begin(), remoteCodecs.end(), [&extendedCodec](const json& remoteCodec) {
            return isRtxCodec(remoteCodec) &&
                   remoteCodec["parameters"]["apt"] == extendedCodec["remotePayloadType"];
          });

				if (jsonRemoteCodecIt == remoteCodecs.end())
					continue;

				auto& matchingRemoteRtxCodec = *jsonRemoteCodecIt;

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

				auto& matchingLocalExt = *jsonLocalExtIt;

				// TODO: Must do stuff for encrypted extensions.

				// clang-format off
				json extendedExt =
				{
					{ "kind",    remoteExt["kind"]                    },
					{ "uri",     remoteExt["uri"]                     },
					{ "sendId",  matchingLocalExt["preferredId"]      },
					{ "recvId",  remoteExt["preferredId"]             },
					{ "encrypt", matchingLocalExt["preferredEncrypt"] }
				};
				// clang-format on

				auto remoteExtDirection = remoteExt["direction"].get<std::string>();

				if (remoteExtDirection == "sendrecv")
					extendedExt["direction"] = "sendrecv";
				else if (remoteExtDirection == "recvonly")
					extendedExt["direction"] = "sendonly";
				else if (remoteExtDirection == "sendonly")
					extendedExt["direction"] = "recvonly";
				else if (remoteExtDirection == "inactive")
					extendedExt["direction"] = "inactive";

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

			// clang-format off
			json rtpCapabilities =
			{
				{ "codecs",           json::array() },
				{ "headerExtensions", json::array() },
				{ "fecMechanisms",    json::array() }
			};
			// clang-format on

			for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				// clang-format off
				json codec =
				{
					{ "mimeType",             extendedCodec["mimeType"]          },
					{ "kind",                 extendedCodec["kind"]              },
					{ "preferredPayloadType", extendedCodec["remotePayloadType"] },
					{ "clockRate",            extendedCodec["clockRate"]         },
					{ "channels",             extendedCodec["channels"]          },
					{ "parameters",           extendedCodec["localParameters"]   },
					{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]      },
				};
				// clang-format on

				rtpCapabilities["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["remoteRtxPayloadType"] != nullptr)
				{
					auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

					// clang-format off
					json rtxCodec =
					{
						{ "mimeType",             mimeType                              },
						{ "kind",                 extendedCodec["kind"]                 },
						{ "preferredPayloadType", extendedCodec["remoteRtxPayloadType"] },
						{ "clockRate",            extendedCodec["clockRate"]            },
						{ "channels",             1                                     },
						{
							"parameters",
							{
								{ "apt", extendedCodec["remotePayloadType"].get<uint8_t>() }
							}
						},
						{ "rtcpFeedback", json::array() }
					};
					// clang-format on

					rtpCapabilities["codecs"].push_back(rtxCodec);
				}

				// TODO: In the future, we need to add FEC, CN, etc, codecs.
			}

			for (auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
			{
				std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for receiving.
				if (direction != "sendrecv" && direction != "recvonly")
					continue;

				// clang-format off
				json ext =
				{
					{ "kind",             extendedExtension["kind"]      },
					{ "uri",              extendedExtension["uri"]       },
					{ "preferredId",      extendedExtension["recvId"]    },
					{ "preferredEncrypt", extendedExtension["encrypt"]   },
					{ "direction",        extendedExtension["direction"] }
				};
				// clang-format on

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

			// clang-format off
			json rtpParameters =
			{
				{ "mid",              nullptr        },
				{ "codecs",           json::array()  },
				{ "headerExtensions", json::array()  },
				{ "encodings",        json::array()  },
				{ "rtcp",             json::object() }
			};
			// clang-format on

			for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				if (kind != extendedCodec["kind"].get<std::string>())
					continue;

				// clang-format off
				json codec =
				{
					{ "mimeType",     extendedCodec["mimeType"]         },
					{ "payloadType",  extendedCodec["localPayloadType"] },
					{ "clockRate",    extendedCodec["clockRate"]        },
					{ "channels",     extendedCodec["channels"]         },
					{ "parameters",   extendedCodec["localParameters"]  },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				rtpParameters["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["localRtxPayloadType"] != nullptr)
				{
					auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

					// clang-format off
					json rtxCodec =
					{
						{ "mimeType",    mimeType                             },
						{ "payloadType", extendedCodec["localRtxPayloadType"] },
						{ "clockRate",   extendedCodec["clockRate"]           },
						{ "channels",    1                                    },
						{
							"parameters",
							{
								{ "apt", extendedCodec["localPayloadType"].get<uint8_t>() }
							}
						},
						{ "rtcpFeedback", json::array() }
					};
					// clang-format on

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

				std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for sending.
				if (direction != "sendrecv" && direction != "sendonly")
					continue;

				// clang-format off
				json ext =
				{
					{ "uri",       extendedExtension["uri"]       },
					{ "id",        extendedExtension["recvId"]    },
					{ "encrypt",   extendedExtension["encrypt"]   },
					{ "direction", extendedExtension["direction"] }
				};
				// clang-format on

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

			// clang-format off
			json rtpParameters =
			{
				{ "mid",              nullptr        },
				{ "codecs",           json::array()  },
				{ "headerExtensions", json::array()  },
				{ "encodings",        json::array()  },
				{ "rtcp",             json::object() }
			};
			// clang-format on

			for (auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				if (kind != extendedCodec["kind"].get<std::string>())
					continue;

				// clang-format off
				json codec =
				{
					{ "mimeType",     extendedCodec["mimeType"]         },
					{ "clockRate",    extendedCodec["clockRate"]        },
					{ "channels",     extendedCodec["channels"]         },
					{ "payloadType",  extendedCodec["localPayloadType"] },
					{ "parameters",   extendedCodec["remoteParameters"] },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				rtpParameters["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["localRtxPayloadType"] != nullptr)
				{
					auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

					// clang-format off
					json rtxCodec =
					{
						{ "mimeType",    mimeType                             },
						{ "payloadType", extendedCodec["localRtxPayloadType"] },
						{ "clockRate",   extendedCodec["clockRate"]           },
						{ "channels",    1                                    },
						{
							"parameters",
							{
								{ "apt", extendedCodec["localPayloadType"].get<uint8_t>() }
							}
						},
						{ "rtcpFeedback", json::array() }
					};
					// clang-format on

					rtpParameters["codecs"].push_back(rtxCodec);
				}

				// NOTE: We assume a single media codec plus an optional RTX codec.
				break;
			}

			for (auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
			{
				if (kind != extendedExtension["kind"].get<std::string>())
					continue;

				std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for sending.
				if (direction != "sendrecv" && direction != "sendonly")
					continue;

				// clang-format off
				json ext =
				{
					{ "uri",       extendedExtension["uri"]       },
					{ "id",        extendedExtension["recvId"]    },
					{ "encrypt",   extendedExtension["encrypt"]   },
					{ "direction", extendedExtension["direction"] }
				};
				// clang-format on

				rtpParameters["headerExtensions"].push_back(ext);
			}

			auto headerExtensionsIt = rtpParameters.find("headerExtensions");

			// Reduce codecs' RTCP feedback. Use Transport-CC if available, REMB otherwise.
			auto headerExtensionIt =
			  std::find_if(headerExtensionsIt->begin(), headerExtensionsIt->end(), [](json& ext) {
				  return ext["uri"].get<std::string>() ==
				         "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
			  });

			if (headerExtensionIt != headerExtensionsIt->end())
			{
				for (auto& codec : rtpParameters["codecs"])
				{
					auto& rtcpFeedback = codec["rtcpFeedback"];

					for (auto it = rtcpFeedback.begin(); it != rtcpFeedback.end();)
					{
						auto& fb  = *it;
						auto type = fb["type"].get<std::string>();

						if (type == "goog-remb")
							it = fb.erase(it);
						else
							++it;
					}
				}

				return rtpParameters;
			}

			headerExtensionIt =
			  std::find_if(headerExtensionsIt->begin(), headerExtensionsIt->end(), [](json& ext) {
				  return ext["uri"].get<std::string>() ==
				         "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
			  });

			if (headerExtensionIt != headerExtensionsIt->end())
			{
				for (auto& codec : rtpParameters["codecs"])
				{
					auto& rtcpFeedback = codec["rtcpFeedback"];

					for (auto it = rtcpFeedback.begin(); it != rtcpFeedback.end();)
					{
						auto& fb  = *it;
						auto type = fb["type"].get<std::string>();

						if (type == "transport-cc")
							it = rtcpFeedback.erase(it);
						else
							++it;
					}
				}

				return rtpParameters;
			}

			for (auto& codec : rtpParameters["codecs"])
			{
				auto& rtcpFeedback = codec["rtcpFeedback"];

				for (auto it = rtcpFeedback.begin(); it != rtcpFeedback.end();)
				{
					auto& fb  = *it;
					auto type = fb["type"].get<std::string>();

					if (type == "transport-cc" || type == "goog-remb")
						it = rtcpFeedback.erase(it);
					else
						++it;
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

			auto& codecs     = extendedRtpCapabilities["codecs"];
			auto jsonCodecIt = std::find_if(codecs.begin(), codecs.end(), [&kind](const json& codec) {
				return kind == codec["kind"].get<std::string>();
			});

			return jsonCodecIt != codecs.end();
		}

		/**
		 * Whether the given RTP parameters can be received with the given RTP
		 * capabilities.
		 */
		bool canReceive(json& rtpParameters, const json& extendedRtpCapabilities)
		{
			MSC_TRACE();

			// This may throw.
			validateRtpParameters(rtpParameters);

			if (rtpParameters["codecs"].empty())
				return false;

			auto& firstMediaCodec = rtpParameters["codecs"][0];
			auto& codecs          = extendedRtpCapabilities["codecs"];
			auto jsonCodecIt =
			  std::find_if(codecs.begin(), codecs.end(), [&firstMediaCodec](const json& codec) {
				  return codec["remotePayloadType"] == firstMediaCodec["payloadType"];
			  });

			return jsonCodecIt != codecs.end();
		}
	} // namespace ortc
} // namespace mediasoupclient

// Private helpers used in this file.

static bool isRtxCodec(const json& codec)
{
	MSC_TRACE();

	static const std::regex RtxMimeTypeRegex(
	  "^(audio|video)/rtx$", std::regex_constants::ECMAScript | std::regex_constants::icase);

	std::smatch match;
	auto mimeType = codec["mimeType"].get<std::string>();

	return std::regex_match(mimeType, match, RtxMimeTypeRegex);
}

static bool matchCodecs(json& aCodec, const json& bCodec, bool strict, bool modify)
{
	MSC_TRACE();

	auto aMimeTypeIt = aCodec.find("mimeType");

	if (aMimeTypeIt == aCodec.end() || !aMimeTypeIt->is_string())
		return false;

	auto bMimeTypeIt = bCodec.find("mimeType");

	if (bMimeTypeIt == bCodec.end() || !bMimeTypeIt->is_string())
		return false;

	auto aMimeType = aMimeTypeIt->get<std::string>();
	auto bMimeType = bMimeTypeIt->get<std::string>();

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

		auto aProfileLevelId = getH264ProfileLevelId(aCodec);
		auto bProfileLevelId = getH264ProfileLevelId(bCodec);

		if (aProfileLevelId.empty() || bProfileLevelId.empty())
			return false;

		webrtc::H264::CodecParameterMap aParameters;
		webrtc::H264::CodecParameterMap bParameters;

		// Check H264 profile.
		aParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(aCodec));
		aParameters["packetization-mode"]      = std::to_string(aPacketizationMode);
		aParameters["profile-level-id"]        = aProfileLevelId;

		bParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(bCodec));
		bParameters["packetization-mode"]      = std::to_string(bPacketizationMode);
		bParameters["profile-level-id"]        = bProfileLevelId;

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
	auto jsonRtcpFeedbackAIt = codecA.find("rtcpFeedback");
	auto jsonRtcpFeedbackBIt = codecB.find("rtcpFeedback");

	for (auto& aFb : *jsonRtcpFeedbackAIt)
	{
		auto jsonRtcpFeedbackIt =
		  std::find_if(jsonRtcpFeedbackBIt->begin(), jsonRtcpFeedbackBIt->end(), [&aFb](const json& bFb) {
			  return (aFb["type"] == bFb["type"] && aFb["parameter"] == bFb["parameter"]);
		  });

		if (jsonRtcpFeedbackIt != jsonRtcpFeedbackBIt->end())
			reducedRtcpFeedback.push_back(*jsonRtcpFeedbackIt);
	}

	return reducedRtcpFeedback;
}

static uint8_t getH264PacketizationMode(const json& codec)
{
	MSC_TRACE();

	auto jsonParametersIt = codec.find("parameters");

	if (jsonParametersIt == codec.end() || !jsonParametersIt->is_object())
		return 0;

	auto jsonPacketizationModeIt = jsonParametersIt->find("packetization-mode");

	// clang-format off
	if (
		jsonPacketizationModeIt == jsonParametersIt->end() ||
		!jsonPacketizationModeIt->is_number()
	)
	// clang-format on
	{
		return 0;
	}

	return jsonPacketizationModeIt->get<uint8_t>();
}

static uint8_t getH264LevelAssimetryAllowed(const json& codec)
{
	MSC_TRACE();

	auto jsonParametersIt = codec.find("parameters");

	if (jsonParametersIt == codec.end() || !jsonParametersIt->is_object())
		return 0;

	auto jsonLevelAssimetryAllowedIt = jsonParametersIt->find("level-assimetry-allowed");

	// clang-format off
	if (
		jsonLevelAssimetryAllowedIt == jsonParametersIt->end() ||
		!jsonLevelAssimetryAllowedIt->is_number_integer()
	)
	// clang-format on
	{
		return 0;
	}

	return jsonLevelAssimetryAllowedIt->get<uint8_t>();
}

static std::string getH264ProfileLevelId(const json& codec)
{
	MSC_TRACE();

	auto jsonParametersIt = codec.find("parameters");

	if (jsonParametersIt == codec.end() || !jsonParametersIt->is_object())
		return "";

	auto jsonAprofileLevelIdIt = jsonParametersIt->find("profile-level-id");

	if (jsonAprofileLevelIdIt == jsonParametersIt->end())
		return "";

	if (jsonAprofileLevelIdIt->is_number())
		return std::to_string(jsonAprofileLevelIdIt->get<uint32_t>());
	else
		return jsonAprofileLevelIdIt->get<std::string>();
}
