#define MSC_CLASS "ortc"

#include "ortc.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include "media/base/sdp_video_format_utils.h"
#include <api/video_codecs/h264_profile_level_id.h>
#include <algorithm> // std::find_if
#include <regex>
#include <stdexcept>

using nlohmann::json;
static constexpr uint32_t ProbatorSsrc{ 1234 };
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
static const std::string ProbatorMid("probator");

// Static functions declaration.
static bool isRtxCodec(const json& codec);
static bool matchCodecs(json& aCodec, json& bCodec, bool strict = false, bool modify = false);
static bool matchHeaderExtensions(const json& aExt, const json& bExt);
static json reduceRtcpFeedback(const json& codecA, const json& codecB);
static uint8_t getH264PacketizationMode(const json& codec);
static uint8_t getH264LevelAssimetryAllowed(const json& codec);
static std::string getH264ProfileLevelId(const json& codec);
static std::string getVP9ProfileId(const json& codec);

namespace mediasoupclient
{
	namespace ortc
	{
		/**
		 * Validates RtpCapabilities. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateRtpCapabilities(json& caps)
		{
			MSC_TRACE();

			if (!caps.is_object())
			{
				MSC_THROW_TYPE_ERROR("caps is not an object");
			}

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
			{
				MSC_THROW_TYPE_ERROR("codec is not an object");
			}

			const auto mimeTypeIt             = codec.find("mimeType");
			const auto preferredPayloadTypeIt = codec.find("preferredPayloadType");
			const auto clockRateIt            = codec.find("clockRate");
			const auto channelsIt             = codec.find("channels");
			auto parametersIt                 = codec.find("parameters");
			auto rtcpFeedbackIt               = codec.find("rtcpFeedback");

			// mimeType is mandatory.
			if (mimeTypeIt == codec.end() || !mimeTypeIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("missing codec.mimeType");
			}

			const std::string regexTarget = mimeTypeIt->get<std::string>();

			std::smatch mimeTypeMatch;
			std::regex_match(regexTarget, mimeTypeMatch, MimeTypeRegex);

			if (mimeTypeMatch.empty())
			{
				MSC_THROW_TYPE_ERROR("invalid codec.mimeType");
			}

			// Just override kind with media component of mimeType.
			// NOTE: The regex always has this group when it matches.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			codec["kind"] = mimeTypeMatch[1].str();

			// preferredPayloadType is optional.
			if (preferredPayloadTypeIt != codec.end() && !preferredPayloadTypeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid codec.preferredPayloadType");
			}

			// clockRate is mandatory.
			if (clockRateIt == codec.end() || !clockRateIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing codec.clockRate");
			}

			// channels is optional. If unset, set it to 1 (just if audio).
			if (codec["kind"] == "audio")
			{
				if (channelsIt == codec.end() || !channelsIt->is_number_integer())
				{
					codec["channels"] = 1;
				}
			}
			else
			{
				if (channelsIt != codec.end())
				{
					codec.erase("channels");
				}
			}

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == codec.end() || !parametersIt->is_object())
			{
				codec["parameters"] = json::object();
				parametersIt        = codec.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				const auto& key   = it.key();
				const auto& value = it.value();

				if (!value.is_string() && !value.is_number() && value != nullptr)
				{
					MSC_THROW_TYPE_ERROR("invalid codec parameter");
				}

				// Specific parameters validation.
				if ((key == "apt") && (!value.is_number_integer()))

				{
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
			{
				MSC_THROW_TYPE_ERROR("fb is not an object");
			}

			const auto typeIt      = fb.find("type");
			const auto parameterIt = fb.find("parameter");

			// type is mandatory.
			if (typeIt == fb.end() || !typeIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("missing fb.type");
			}

			// parameter is optional. If unset set it to an empty string.
			if (parameterIt == fb.end() || !parameterIt->is_string())
			{
				fb["parameter"] = "";
			}
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
			{
				MSC_THROW_TYPE_ERROR("ext is not an object");
			}

			const auto uriIt              = ext.find("uri");
			const auto preferredIdIt      = ext.find("preferredId");
			const auto preferredEncryptIt = ext.find("preferredEncrypt");
			const auto directionIt        = ext.find("direction");

			auto kindIt = ext.find("kind");

			// kind is mandatory.
			if (kindIt == ext.end() || !kindIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("missing ext.kind");
			}

			kindIt                 = ext.find("kind");
			const std::string kind = kindIt->get<std::string>();

			if (kind != "audio" && kind != "video")
			{
				MSC_THROW_TYPE_ERROR("invalid ext.kind");
			}

			// uri is mandatory.
			if (uriIt == ext.end() || !uriIt->is_string() || uriIt->get<std::string>().empty())
			{
				MSC_THROW_TYPE_ERROR("missing ext.uri");
			}

			// preferredId is mandatory.
			if (preferredIdIt == ext.end() || !preferredIdIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing ext.preferredId");
			}

			// preferredEncrypt is optional. If unset set it to false.
			if (preferredEncryptIt != ext.end() && !preferredEncryptIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid ext.preferredEncrypt");
			}
			else if (preferredEncryptIt == ext.end())
			{
				ext["preferredEncrypt"] = false;
			}

			// direction is optional. If unset set it to sendrecv.
			if (directionIt != ext.end() && !directionIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("invalid ext.direction");
			}
			else if (directionIt == ext.end())
			{
				ext["direction"] = "sendrecv";
			}
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
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto midIt        = params.find("mid");
			const auto codecsIt     = params.find("codecs");
			auto headerExtensionsIt = params.find("headerExtensions");
			auto encodingsIt        = params.find("encodings");
			auto rtcpIt             = params.find("rtcp");

			// mid is optional.
			if (midIt != params.end() && (!midIt->is_string() || midIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("params.mid is not a string");
			}

			// codecs is mandatory.
			if (codecsIt == params.end() || !codecsIt->is_array())
			{
				MSC_THROW_TYPE_ERROR("missing params.codecs");
			}

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
			{
				MSC_THROW_TYPE_ERROR("codec is not an object");
			}

			const auto mimeTypeIt    = codec.find("mimeType");
			const auto payloadTypeIt = codec.find("payloadType");
			const auto clockRateIt   = codec.find("clockRate");
			const auto channelsIt    = codec.find("channels");
			auto parametersIt        = codec.find("parameters");
			auto rtcpFeedbackIt      = codec.find("rtcpFeedback");

			// mimeType is mandatory.
			if (mimeTypeIt == codec.end() || !mimeTypeIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("missing codec.mimeType");
			}

			const std::string regexTarget = mimeTypeIt->get<std::string>();

			std::smatch mimeTypeMatch;
			std::regex_match(regexTarget, mimeTypeMatch, MimeTypeRegex);

			if (mimeTypeMatch.empty())
			{
				MSC_THROW_TYPE_ERROR("invalid codec.mimeType");
			}

			// payloadType is mandatory.
			if (payloadTypeIt == codec.end() || !payloadTypeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing codec.payloadType");
			}

			// clockRate is mandatory.
			if (clockRateIt == codec.end() || !clockRateIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing codec.clockRate");
			}

			// Retrieve media kind from mimeType.
			// NOTE: The regex always has this group when it matches.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			const auto kind = mimeTypeMatch[1].str();

			// channels is optional. If unset, set it to 1 (just for audio).
			if (kind == "audio")
			{
				if (channelsIt == codec.end() || !channelsIt->is_number_integer())
				{
					codec["channels"] = 1;
				}
			}
			else
			{
				if (channelsIt != codec.end())
				{
					codec.erase("channels");
				}
			}

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == codec.end() || !parametersIt->is_object())
			{
				codec["parameters"] = json::object();
				parametersIt        = codec.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				const auto& key   = it.key();
				const auto& value = it.value();

				if (!value.is_string() && !value.is_number() && value != nullptr)
				{
					MSC_THROW_TYPE_ERROR("invalid codec parameter");
				}

				// Specific parameters validation.
				if ((key == "apt") && (!value.is_number_integer()))

				{
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
			{
				MSC_THROW_TYPE_ERROR("ext is not an object");
			}

			const auto uriIt     = ext.find("uri");
			const auto idIt      = ext.find("id");
			const auto encryptIt = ext.find("encrypt");
			auto parametersIt    = ext.find("parameters");

			// uri is mandatory.
			if (uriIt == ext.end() || !uriIt->is_string() || uriIt->get<std::string>().empty())
			{
				MSC_THROW_TYPE_ERROR("missing ext.uri");
			}

			// id is mandatory.
			if (idIt == ext.end() || !idIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing ext.id");
			}

			// encrypt is optional. If unset set it to false.
			if (encryptIt != ext.end() && !encryptIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid ext.encrypt");
			}
			else if (encryptIt == ext.end())
			{
				ext["encrypt"] = false;
			}

			// parameters is optional. If unset, set it to an empty object.
			if (parametersIt == ext.end() || !parametersIt->is_object())
			{
				ext["parameters"] = json::object();
				parametersIt      = ext.find("parameters");
			}

			for (auto it = parametersIt->begin(); it != parametersIt->end(); ++it)
			{
				const auto& value = it.value();

				if (!value.is_string() && !value.is_number())
				{
					MSC_THROW_TYPE_ERROR("invalid header extension parameter");
				}
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
			{
				MSC_THROW_TYPE_ERROR("encoding is not an object");
			}

			const auto ssrcIt            = encoding.find("ssrc");
			const auto ridIt             = encoding.find("rid");
			const auto rtxIt             = encoding.find("rtx");
			const auto dtxIt             = encoding.find("dtx");
			const auto scalabilityModeIt = encoding.find("scalabilityMode");

			// ssrc is optional.
			if (ssrcIt != encoding.end() && !ssrcIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid encoding.ssrc");
			}

			// rid is optional.
			if (ridIt != encoding.end() && (!ridIt->is_string() || ridIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("invalid encoding.rid");
			}

			// rtx is optional.
			if (rtxIt != encoding.end() && !rtxIt->is_object())
			{
				MSC_THROW_TYPE_ERROR("invalid encoding.rtx");
			}
			else if (rtxIt != encoding.end())
			{
				const auto rtxSsrcIt = rtxIt->find("ssrc");

				// RTX ssrc is mandatory if rtx is present.
				if (rtxSsrcIt == rtxIt->end() || !rtxSsrcIt->is_number_integer())
				{
					MSC_THROW_TYPE_ERROR("missing encoding.rtx.ssrc");
				}
			}

			// dtx is optional. If unset set it to false.
			if (dtxIt == encoding.end() || !dtxIt->is_boolean())
			{
				encoding["dtx"] = false;
			}

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
			{
				MSC_THROW_TYPE_ERROR("rtcp is not an object");
			}

			const auto cnameIt       = rtcp.find("cname");
			const auto reducedSizeIt = rtcp.find("reducedSize");

			// cname is optional.
			if (cnameIt != rtcp.end() && !cnameIt->is_string())
			{
				MSC_THROW_TYPE_ERROR("invalid rtcp.cname");
			}

			// reducedSize is optional. If unset set it to true.
			if (reducedSizeIt == rtcp.end() || !reducedSizeIt->is_boolean())
			{
				rtcp["reducedSize"] = true;
			}
		}

		/**
		 * Validates SctpCapabilities. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateSctpCapabilities(json& caps)
		{
			MSC_TRACE();

			if (!caps.is_object())
			{
				MSC_THROW_TYPE_ERROR("caps is not an object");
			}

			const auto numStreamsIt = caps.find("numStreams");

			// numStreams is mandatory.
			if (numStreamsIt == caps.end() || !numStreamsIt->is_object())
			{
				MSC_THROW_TYPE_ERROR("missing caps.numStreams");
			}

			ortc::validateNumSctpStreams(*numStreamsIt);
		}

		/**
		 * Validates NumSctpStreams. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateNumSctpStreams(json& numStreams)
		{
			MSC_TRACE();

			if (!numStreams.is_object())
			{
				MSC_THROW_TYPE_ERROR("numStreams is not an object");
			}

			const auto osIt  = numStreams.find("OS");
			const auto misIt = numStreams.find("MIS");

			// OS is mandatory.
			if (osIt == numStreams.end() || !osIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing numStreams.OS");
			}

			// MIS is mandatory.
			if (misIt == numStreams.end() || !misIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing numStreams.MIS");
			}
		}

		/**
		 * Validates SctpParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateSctpParameters(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto portIt               = params.find("port");
			const auto maxSendMessageSizeIt = params.find("maxSendMessageSize");
			const auto maxRecvMessageSizeIt = params.find("maxReceiveMessageSize");

			// port is mandatory.
			if (portIt == params.end() || !portIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing params.port");
			}

			// maxSendMessageSize is optional.
			if (maxSendMessageSizeIt != params.end() && !maxSendMessageSizeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.maxSendMessageSize");
			}

			// maxReceiveMessageSize is optional.
			if (maxRecvMessageSizeIt != params.end() && !maxRecvMessageSizeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.maxReceiveMessageSize");
			}
		}

		/**
		 * Validates SctpStreamParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateSctpStreamParameters(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto streamIdIt          = params.find("streamId");
			const auto orderedIt           = params.find("ordered");
			const auto maxPacketLifeTimeIt = params.find("maxPacketLifeTime");
			const auto maxRetransmitsIt    = params.find("maxRetransmits");
			const auto labelIt             = params.find("label");
			const auto protocolIt          = params.find("protocol");

			// streamId is mandatory.
			if (streamIdIt == params.end() || !streamIdIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing params.streamId");
			}

			// ordered is optional.
			bool orderedGiven = false;

			if (orderedIt != params.end() && orderedIt->is_boolean())
			{
				orderedGiven = true;
			}
			else
			{
				params["ordered"] = true;
			}

			// NOTE: maxPacketLifeTime and maxRetransmits are left unset rather than
			// defaulted to 0, since they are sent to the server, which rejects a
			// sctpStreamParameters carrying both of them.

			// maxPacketLifeTime is optional.
			if (maxPacketLifeTimeIt != params.end() && !maxPacketLifeTimeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.maxPacketLifeTime");
			}

			// maxRetransmits is optional.
			if (maxRetransmitsIt != params.end() && !maxRetransmitsIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.maxRetransmits");
			}

			if (maxPacketLifeTimeIt != params.end() && maxRetransmitsIt != params.end())
			{
				MSC_THROW_TYPE_ERROR("cannot provide both maxPacketLifeTime and maxRetransmits");
			}

			// clang-format off
			if (
				orderedGiven &&
				params["ordered"] == true &&
				(maxPacketLifeTimeIt != params.end() || maxRetransmitsIt != params.end())
			)
			// clang-format on
			{
				MSC_THROW_TYPE_ERROR("cannot be ordered with maxPacketLifeTime or maxRetransmits");
			}
			// clang-format off
			else if (
				!orderedGiven &&
				(maxPacketLifeTimeIt != params.end() || maxRetransmitsIt != params.end())
			)
			// clang-format on
			{
				params["ordered"] = false;
			}

			// label is optional. If unset set it to empty string.
			if (labelIt == params.end() || !labelIt->is_string())
			{
				params["label"] = "";
			}

			// protocol is optional. If unset set it to empty string.
			if (protocolIt == params.end() || !protocolIt->is_string())
			{
				params["protocol"] = "";
			}
		}

		/**
		 * Validates IceParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateIceParameters(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto usernameFragmentIt = params.find("usernameFragment");
			const auto passwordIt         = params.find("password");
			const auto iceLiteIt          = params.find("iceLite");

			// usernameFragment is mandatory.
			if (
			  usernameFragmentIt == params.end() ||
			  (!usernameFragmentIt->is_string() || usernameFragmentIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.usernameFragment");
			}

			// userFragment is mandatory.
			if (passwordIt == params.end() || (!passwordIt->is_string() || passwordIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.password");
			}

			// iceLIte is optional. If unset set it to false.
			if (iceLiteIt == params.end() || !iceLiteIt->is_boolean())
			{
				params["iceLite"] = false;
			}
		}

		/**
		 * Validates IceCandidate. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		static void validateIceCandidate(json& params)
		{
			MSC_TRACE();

			static const std::regex ProtocolRegex(
			  "(udp|tcp)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			static const std::regex TypeRegex(
			  "(host|srflx|prflx|relay)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto foundationIt = params.find("foundation");
			const auto priorityIt   = params.find("priority");
			const auto ipIt         = params.find("ip");
			const auto protocolIt   = params.find("protocol");
			const auto portIt       = params.find("port");
			const auto typeIt       = params.find("type");

			// foundation is mandatory.
			if (
			  foundationIt == params.end() ||
			  (!foundationIt->is_string() || foundationIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.foundation");
			}

			// priority is mandatory.
			if (priorityIt == params.end() || !priorityIt->is_number_unsigned())
			{
				MSC_THROW_TYPE_ERROR("missing params.priority");
			}

			// ip is mandatory.
			if (ipIt == params.end() || (!ipIt->is_string() || ipIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.ip");
			}

			// protocol is mandatory.
			if (protocolIt == params.end() || (!protocolIt->is_string() || protocolIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.protocol");
			}

			std::smatch protocolMatch;
			std::string regexTarget = protocolIt->get<std::string>();
			std::regex_match(regexTarget, protocolMatch, ProtocolRegex);

			if (protocolMatch.empty())
			{
				MSC_THROW_TYPE_ERROR("invalid params.protocol");
			}

			// port is mandatory.
			if (portIt == params.end() || !portIt->is_number_unsigned())
			{
				MSC_THROW_TYPE_ERROR("missing params.port");
			}

			// type is mandatory.
			if (typeIt == params.end() || (!typeIt->is_string() || typeIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.type");
			}

			std::smatch typeMatch;
			regexTarget = typeIt->get<std::string>();
			std::regex_match(regexTarget, typeMatch, TypeRegex);

			if (typeMatch.empty())
			{
				MSC_THROW_TYPE_ERROR("invalid params.type");
			}
		}

		/**
		 * Validates IceCandidates. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateIceCandidates(json& params)
		{
			MSC_TRACE();

			if (!params.is_array())
			{
				MSC_THROW_TYPE_ERROR("params is not an array");
			}

			for (auto& iceCandidate : params)
			{
				validateIceCandidate(iceCandidate);
			}
		}

		/**
		 * Validates DtlsFingerprint. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		static void validateDtlsFingerprint(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto algorithmIt = params.find("algorithm");
			const auto valueIt     = params.find("value");

			// foundation is mandatory.
			if (
			  algorithmIt == params.end() ||
			  (!algorithmIt->is_string() || algorithmIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.algorithm");
			}

			// foundation is mandatory.
			if (valueIt == params.end() || (!valueIt->is_string() || valueIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.value");
			}
		}

		/**
		 * Validates DtlsParameters. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateDtlsParameters(json& params)
		{
			MSC_TRACE();

			static const std::regex RoleRegex(
			  "(auto|client|server)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto roleIt         = params.find("role");
			const auto fingerprintsIt = params.find("fingerprints");

			// role is mandatory.
			if (roleIt == params.end() || (!roleIt->is_string() || roleIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.role");
			}

			const std::string regexTarget = roleIt->get<std::string>();

			std::smatch roleMatch;
			std::regex_match(regexTarget, roleMatch, RoleRegex);

			if (roleMatch.empty())
			{
				MSC_THROW_TYPE_ERROR("invalid params.role");
			}

			// fingerprints is mandatory.
			if (fingerprintsIt == params.end() || (!fingerprintsIt->is_array() || fingerprintsIt->empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.fingerprints");
			}

			for (auto& fingerprint : *fingerprintsIt)
			{
				validateDtlsFingerprint(fingerprint);
			}
		}

		/**
		 * Validates Producer codec options. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateProducerCodecOptions(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
			{
				MSC_THROW_TYPE_ERROR("params is not an object");
			}

			const auto opusStereoIt              = params.find("opusStereo");
			const auto opusFecIt                 = params.find("opusFec");
			const auto opusDtxIt                 = params.find("opusDtx");
			const auto opusCbrIt                 = params.find("opusCbr");
			const auto opusMaxPlaybackRateIt     = params.find("opusMaxPlaybackRate");
			const auto opusMaxAverageBitrateIt   = params.find("opusMaxAverageBitrate");
			const auto opusPtimeIt               = params.find("opusPtime");
			const auto videoGoogleStartBitrateIt = params.find("videoGoogleStartBitrate");
			const auto videoGoogleMaxBitrateIt   = params.find("videoGoogleMaxBitrate");
			const auto videoGoogleMinBitrateIt   = params.find("videoGoogleMinBitrate");

			if (opusStereoIt != params.end() && !opusStereoIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusStereo");
			}

			if (opusFecIt != params.end() && !opusFecIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusFec");
			}

			if (opusDtxIt != params.end() && !opusDtxIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusDtx");
			}

			if (opusCbrIt != params.end() && !opusCbrIt->is_boolean())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusCbr");
			}

			if (opusMaxPlaybackRateIt != params.end() && !opusMaxPlaybackRateIt->is_number_unsigned())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusMaxPlaybackRate");
			}

			if (opusMaxAverageBitrateIt != params.end() && !opusMaxAverageBitrateIt->is_number_unsigned())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusMaxAverageBitrate");
			}

			if (opusPtimeIt != params.end() && !opusPtimeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusPtime");
			}

			if (videoGoogleStartBitrateIt != params.end() && !videoGoogleStartBitrateIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.videoGoogleStartBitrate");
			}

			if (videoGoogleMaxBitrateIt != params.end() && !videoGoogleMaxBitrateIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.videoGoogleMaxBitrate");
			}

			if (videoGoogleMinBitrateIt != params.end() && !videoGoogleMinBitrateIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("invalid params.videoGoogleMinBitrate");
			}
		}

		/**
		 * Generate extended RTP capabilities for sending and receiving.
		 */
		json getExtendedRtpCapabilities(json& localCaps, json& remoteCaps, bool preferLocalCodecsOrder)
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
				{ "headerExtensions", json::array() }
			};
			// clang-format on

			// Match media codecs.
			if (preferLocalCodecsOrder)
			{
				// Keep order preferred by local capabilities.
				json& localCodecs = localCaps["codecs"];

				for (auto& localCodec : localCodecs)
				{
					if (isRtxCodec(localCodec))
					{
						continue;
					}

					auto& remoteCodecs = remoteCaps["codecs"];

					const auto matchingRemoteCodecIt = std::ranges::find_if(
					  remoteCodecs,

					  [&localCodec](json& remoteCodec) -> bool
					  {
						  return matchCodecs(remoteCodec, localCodec, /*strict*/ true, /*modify*/ true);
					  });

					if (matchingRemoteCodecIt == remoteCodecs.end())
					{
						continue;
					}

					auto& matchingRemoteCodec = *matchingRemoteCodecIt;

					// clang-format off
					json extendedCodec =
					{
						{ "mimeType",             localCodec["mimeType"]                          },
						{ "kind",                 localCodec["kind"]                              },
						{ "clockRate",            localCodec["clockRate"]                         },
						{ "localPayloadType",     localCodec["preferredPayloadType"]              },
						{ "localRtxPayloadType",  nullptr                                         },
						{ "remotePayloadType",    matchingRemoteCodec["preferredPayloadType"]     },
						{ "remoteRtxPayloadType", nullptr                                         },
						{ "localParameters",      localCodec["parameters"]                        },
						{ "remoteParameters",     matchingRemoteCodec["parameters"]               },
						{ "rtcpFeedback",         reduceRtcpFeedback(localCodec, matchingRemoteCodec) }
					};
					// clang-format on

					if (localCodec.contains("channels"))
					{
						extendedCodec["channels"] = localCodec["channels"];
					}

					extendedRtpCapabilities["codecs"].push_back(extendedCodec);
				}
			}
			else
			{
				// Keep order preferred by remote capabilities.
				const auto remoteCapsCodecsIt = remoteCaps.find("codecs");

				for (auto& remoteCodec : *remoteCapsCodecsIt)
				{
					if (isRtxCodec(remoteCodec))
					{
						continue;
					}

					json& localCodecs = localCaps["codecs"];

					const auto matchingLocalCodecIt = std::ranges::find_if(
					  localCodecs,

					  [&remoteCodec](json& localCodec) -> bool
					  {
						  return matchCodecs(localCodec, remoteCodec, /*strict*/ true, /*modify*/ true);
					  });

					if (matchingLocalCodecIt == localCodecs.end())
					{
						continue;
					}

					auto& matchingLocalCodec = *matchingLocalCodecIt;

					// clang-format off
					json extendedCodec =
					{
						{ "mimeType",             matchingLocalCodec["mimeType"]                      },
						{ "kind",                 matchingLocalCodec["kind"]                          },
						{ "clockRate",            matchingLocalCodec["clockRate"]                     },
						{ "localPayloadType",     matchingLocalCodec["preferredPayloadType"]          },
						{ "localRtxPayloadType",  nullptr                                             },
						{ "remotePayloadType",    remoteCodec["preferredPayloadType"]                 },
						{ "remoteRtxPayloadType", nullptr                                             },
						{ "localParameters",      matchingLocalCodec["parameters"]                    },
						{ "remoteParameters",     remoteCodec["parameters"]                           },
						{ "rtcpFeedback",         reduceRtcpFeedback(matchingLocalCodec, remoteCodec) }
					};
					// clang-format on

					if (matchingLocalCodec.contains("channels"))
					{
						extendedCodec["channels"] = matchingLocalCodec["channels"];
					}

					extendedRtpCapabilities["codecs"].push_back(extendedCodec);
				}
			}

			// Match RTX codecs.
			json& extendedCodecs = extendedRtpCapabilities["codecs"];

			for (json& extendedCodec : extendedCodecs)
			{
				auto& localCodecs = localCaps["codecs"];

				const auto localCodecIt = std::ranges::find_if(
				  localCodecs,

				  [&extendedCodec](const json& localCodec) -> bool
				  {
					  return isRtxCodec(localCodec) &&
						       localCodec["parameters"]["apt"] == extendedCodec["localPayloadType"];
				  });

				if (localCodecIt == localCodecs.end())
				{
					continue;
				}

				auto& matchingLocalRtxCodec = *localCodecIt;
				auto& remoteCodecs          = remoteCaps["codecs"];

				const auto remoteCodecIt = std::ranges::find_if(
				  remoteCodecs,

				  [&extendedCodec](const json& remoteCodec) -> bool
				  {
					  return isRtxCodec(remoteCodec) &&
						       remoteCodec["parameters"]["apt"] == extendedCodec["remotePayloadType"];
				  });

				if (remoteCodecIt == remoteCodecs.end())
				{
					continue;
				}

				auto& matchingRemoteRtxCodec = *remoteCodecIt;

				extendedCodec["localRtxPayloadType"]  = matchingLocalRtxCodec["preferredPayloadType"];
				extendedCodec["remoteRtxPayloadType"] = matchingRemoteRtxCodec["preferredPayloadType"];
			}

			// Match header extensions.
			auto& remoteExts = remoteCaps["headerExtensions"];

			for (auto& remoteExt : remoteExts)
			{
				auto& localExts = localCaps["headerExtensions"];

				const auto localExtIt = std::ranges::find_if(
				  localExts,

				  [&remoteExt](const json& localExt) -> bool
				  {
					  return matchHeaderExtensions(localExt, remoteExt);
				  });

				if (localExtIt == localExts.end())
				{
					continue;
				}

				auto& matchingLocalExt = *localExtIt;

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

				const auto remoteExtDirection = remoteExt["direction"].get<std::string>();

				if (remoteExtDirection == "sendrecv")
				{
					extendedExt["direction"] = "sendrecv";
				}
				else if (remoteExtDirection == "recvonly")
				{
					extendedExt["direction"] = "sendonly";
				}
				else if (remoteExtDirection == "sendonly")
				{
					extendedExt["direction"] = "recvonly";
				}
				else if (remoteExtDirection == "inactive")
				{
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

			// clang-format off
			json rtpCapabilities =
			{
				{ "codecs",           json::array() },
				{ "headerExtensions", json::array() }
			};
			// clang-format on

			for (const auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				// clang-format off
				json codec =
				{
					{ "mimeType",             extendedCodec["mimeType"]          },
					{ "kind",                 extendedCodec["kind"]              },
					{ "preferredPayloadType", extendedCodec["remotePayloadType"] },
					{ "clockRate",            extendedCodec["clockRate"]         },
					{ "parameters",           extendedCodec["localParameters"]   },
					{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]      },
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
				{
					codec["channels"] = extendedCodec["channels"];
				}

				rtpCapabilities["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["remoteRtxPayloadType"] == nullptr)
				{
					continue;
				}

				auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

				// clang-format off
				json const rtxCodec =
				{
					{ "mimeType",             mimeType                              },
					{ "kind",                 extendedCodec["kind"]                 },
					{ "preferredPayloadType", extendedCodec["remoteRtxPayloadType"] },
					{ "clockRate",            extendedCodec["clockRate"]            },
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

				// TODO: In the future, we need to add FEC, CN, etc, codecs.
			}

			for (const auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
			{
				const std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for receiving.
				if (direction != "sendrecv" && direction != "recvonly")
				{
					continue;
				}

				// clang-format off
				json const ext =
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

			return rtpCapabilities;
		}

		/**
		 * Generate RTP parameters of the given kind for sending media.
		 * All media codecs with matching kind are considered.
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

			for (const auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				if (kind != extendedCodec["kind"].get<std::string>())
				{
					continue;
				}

				// clang-format off
				json codec =
				{
					{ "mimeType",     extendedCodec["mimeType"]         },
					{ "payloadType",  extendedCodec["localPayloadType"] },
					{ "clockRate",    extendedCodec["clockRate"]        },
					{ "parameters",   extendedCodec["localParameters"]  },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
				{
					codec["channels"] = extendedCodec["channels"];
				}

				rtpParameters["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["localRtxPayloadType"] != nullptr)
				{
					auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

					// clang-format off
					json const rtxCodec =
					{
						{ "mimeType",    mimeType                             },
						{ "payloadType", extendedCodec["localRtxPayloadType"] },
						{ "clockRate",   extendedCodec["clockRate"]           },
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
			}

			for (const auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
			{
				if (kind != extendedExtension["kind"].get<std::string>())
				{
					continue;
				}

				const std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for sending.
				if (direction != "sendrecv" && direction != "sendonly")
				{
					continue;
				}

				// clang-format off
				json const ext =
				{
					{ "uri",        extendedExtension["uri"]     },
					{ "id",         extendedExtension["sendId"]  },
					{ "encrypt",    extendedExtension["encrypt"] },
					{ "parameters", json::object()               }
				};
				// clang-format on

				rtpParameters["headerExtensions"].push_back(ext);
			}

			return rtpParameters;
		}

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

			for (const auto& extendedCodec : extendedRtpCapabilities["codecs"])
			{
				if (kind != extendedCodec["kind"].get<std::string>())
				{
					continue;
				}

				// clang-format off
				json codec =
				{
					{ "mimeType",     extendedCodec["mimeType"]         },
					{ "payloadType",  extendedCodec["localPayloadType"] },
					{ "clockRate",    extendedCodec["clockRate"]        },
					{ "parameters",   extendedCodec["remoteParameters"] },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
				{
					codec["channels"] = extendedCodec["channels"];
				}

				rtpParameters["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["localRtxPayloadType"] != nullptr)
				{
					auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

					// clang-format off
					json const rtxCodec =
					{
						{ "mimeType",    mimeType                             },
						{ "payloadType", extendedCodec["localRtxPayloadType"] },
						{ "clockRate",   extendedCodec["clockRate"]           },
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
			}

			for (const auto& extendedExtension : extendedRtpCapabilities["headerExtensions"])
			{
				if (kind != extendedExtension["kind"].get<std::string>())
				{
					continue;
				}

				const std::string direction = extendedExtension["direction"].get<std::string>();

				// Ignore RTP extensions not valid for sending.
				if (direction != "sendrecv" && direction != "sendonly")
				{
					continue;
				}

				// clang-format off
				json const ext =
				{
					{ "uri",        extendedExtension["uri"]     },
					{ "id",         extendedExtension["sendId"]  },
					{ "encrypt",    extendedExtension["encrypt"] },
					{ "parameters", json::object()               }
				};
				// clang-format on

				rtpParameters["headerExtensions"].push_back(ext);
			}

			const auto headerExtensionsIt = rtpParameters.find("headerExtensions");

			// Reduce codecs' RTCP feedback. Use Transport-CC if available, REMB otherwise.
			auto headerExtensionIt = std::find_if(
			  headerExtensionsIt->begin(),
			  headerExtensionsIt->end(),
			  [](json& ext) -> bool
			  {
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
						auto& fb = *it;

						const auto type = fb["type"].get<std::string>();

						if (type == "goog-remb")
						{
							it = rtcpFeedback.erase(it);
						}
						else
						{
							++it;
						}
					}
				}

				return rtpParameters;
			}

			headerExtensionIt = std::find_if(
			  headerExtensionsIt->begin(),
			  headerExtensionsIt->end(),
			  [](json& ext) -> bool
			  {
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
						auto& fb = *it;

						const auto type = fb["type"].get<std::string>();

						if (type == "transport-cc")
						{
							it = rtcpFeedback.erase(it);
						}
						else
						{
							++it;
						}
					}
				}

				return rtpParameters;
			}

			for (auto& codec : rtpParameters["codecs"])
			{
				auto& rtcpFeedback = codec["rtcpFeedback"];

				for (auto it = rtcpFeedback.begin(); it != rtcpFeedback.end();)
				{
					auto& fb = *it;

					const auto type = fb["type"].get<std::string>();

					if (type == "transport-cc" || type == "goog-remb")
					{
						it = rtcpFeedback.erase(it);
					}
					else
					{
						++it;
					}
				}
			}

			return rtpParameters;
		}

		/**
		 * Create RTP parameters for a Consumer for the RTP probator.
		 */
		json generateProbatorRtpParameters(const json& videoRtpParameters)
		{
			MSC_TRACE();

			// This may throw.
			json validatedRtpParameters = videoRtpParameters;

			// This may throw.
			ortc::validateRtpParameters(validatedRtpParameters);

			// clang-format off
			json rtpParameters =
			{
				{ "mid",              ProbatorMid    },
				{ "codecs",           json::array()  },
				{ "headerExtensions", json::array()  },
				{ "encodings",        json::array()  },
				{
					"rtcp",
					{
						{ "cname", "probator" }
					}
				}
			};
			// clang-format on

			rtpParameters["codecs"].push_back(validatedRtpParameters["codecs"][0]);

			for (auto& ext : validatedRtpParameters["headerExtensions"])
			{
				// clang-format off
				if (
					ext["uri"] == "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time" ||
					ext["uri"] == "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"
				)
				// clang-format on
				{
					rtpParameters["headerExtensions"].push_back(ext);
				}
			}

			json encoding = json::object();

			encoding["ssrc"] = ProbatorSsrc;

			rtpParameters["encodings"].push_back(encoding);

			return rtpParameters;
		}

		/**
		 * Whether media can be sent based on the given RTP capabilities.
		 */
		bool canSend(const std::string& kind, const json& extendedRtpCapabilities)
		{
			MSC_TRACE();

			const auto& codecs = extendedRtpCapabilities["codecs"];
			const auto codecIt = std::ranges::find_if(
			  codecs,

			  [&kind](const json& codec) -> bool
			  {
				  return kind == codec["kind"].get<std::string>();
			  });

			return codecIt != codecs.end();
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
			{
				return false;
			}

			const auto& codecs = extendedRtpCapabilities["codecs"];

			auto& firstMediaCodec = rtpParameters["codecs"][0];

			const auto codecIt = std::ranges::find_if(
			  codecs,

			  [&firstMediaCodec](const json& codec) -> bool
			  {
				  return codec["preferredPayloadType"] == firstMediaCodec["payloadType"];
			  });

			return codecIt != codecs.end();
		}

		nlohmann::json reduceCodecs(nlohmann::json& codecs, const nlohmann::json* capCodec)
		{
			MSC_TRACE();

			nlohmann::json filteredCodecs = nlohmann::json::array();

			// If no capability codec is given, take the first one (and RTX).
			if (!capCodec || !capCodec->is_object())
			{
				if (codecs.empty())
				{
					MSC_THROW_TYPE_ERROR("empty codecs");
				}

				filteredCodecs.push_back(codecs[0]);

				if (codecs.size() > 1 && isRtxCodec(codecs[1]))
				{
					filteredCodecs.push_back(codecs[1]);
				}
			}
			// Otherwise look for a compatible set of codecs.
			else
			{
				for (size_t idx = 0; idx < codecs.size(); ++idx)
				{
					if (matchCodecs(codecs[idx], const_cast<json&>(*capCodec), /*strict*/ true))
					{
						filteredCodecs.push_back(codecs[idx]);

						if (codecs.size() > (idx + 1) && isRtxCodec(codecs[idx + 1]))
						{
							filteredCodecs.push_back(codecs[idx + 1]);
						}

						break;
					}
				}

				if (filteredCodecs.empty())
				{
					MSC_THROW_TYPE_ERROR("no matching codec found");
				}
			}

			return filteredCodecs;
		}

	} // namespace ortc
} // namespace mediasoupclient

// Private helpers used in this file.

static bool isRtxCodec(const json& codec)
{
	MSC_TRACE();

	static const std::regex RtxMimeTypeRegex(
	  "^(audio|video)/rtx$", std::regex_constants::ECMAScript | std::regex_constants::icase);

	const auto mimeType = codec["mimeType"].get<std::string>();

	std::smatch match;

	return std::regex_match(mimeType, match, RtxMimeTypeRegex);
}

static bool matchCodecs(json& aCodec, json& bCodec, bool strict, bool modify)
{
	MSC_TRACE();

	const auto aMimeTypeIt = aCodec.find("mimeType");
	const auto bMimeTypeIt = bCodec.find("mimeType");
	auto aMimeType         = aMimeTypeIt->get<std::string>();
	auto bMimeType         = bMimeTypeIt->get<std::string>();

	std::ranges::transform(aMimeType, aMimeType.begin(), ::tolower);
	std::ranges::transform(bMimeType, bMimeType.begin(), ::tolower);

	if (aMimeType != bMimeType)
	{
		return false;
	}

	if (aCodec["clockRate"] != bCodec["clockRate"])
	{
		return false;
	}

	if (aCodec.contains("channels") != bCodec.contains("channels"))
	{
		return false;
	}

	if (aCodec.contains("channels") && aCodec["channels"] != bCodec["channels"])
	{
		return false;
	}

	// Match H264 parameters.
	if (aMimeType == "video/h264")
	{
		if (strict)
		{
			const auto aPacketizationMode = getH264PacketizationMode(aCodec);
			const auto bPacketizationMode = getH264PacketizationMode(bCodec);

			if (aPacketizationMode != bPacketizationMode)
			{
				return false;
			}

			webrtc::CodecParameterMap aParameters;
			webrtc::CodecParameterMap bParameters;

			aParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(aCodec));
			aParameters["packetization-mode"]      = std::to_string(aPacketizationMode);
			aParameters["profile-level-id"]        = getH264ProfileLevelId(aCodec);
			bParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(bCodec));
			bParameters["packetization-mode"]      = std::to_string(bPacketizationMode);
			bParameters["profile-level-id"]        = getH264ProfileLevelId(bCodec);

			if (!webrtc::H264IsSameProfile(aParameters, bParameters))
			{
				return false;
			}

			webrtc::CodecParameterMap newParameters;

			try
			{
				webrtc::H264GenerateProfileLevelIdForAnswer(
				  aParameters, bParameters, std::addressof(newParameters));
			}
			catch (const std::runtime_error&)
			{
				return false;
			}

			if (modify)
			{
				const auto profileLevelIdIt = newParameters.find("profile-level-id");

				if (profileLevelIdIt != newParameters.end())
				{
					aCodec["parameters"]["profile-level-id"] = profileLevelIdIt->second;
					bCodec["parameters"]["profile-level-id"] = profileLevelIdIt->second;
				}
				else
				{
					aCodec["parameters"].erase("profile-level-id");
					bCodec["parameters"].erase("profile-level-id");
				}
			}
		}
	}
	// Match VP9 parameters.
	else if ((aMimeType == "video/vp9") && strict)

	{
		const auto aProfileId = getVP9ProfileId(aCodec);
		const auto bProfileId = getVP9ProfileId(bCodec);

		if (aProfileId != bProfileId)
		{
			return false;
		}
	}

	return true;
}

static bool matchHeaderExtensions(const json& aExt, const json& bExt)
{
	MSC_TRACE();

	if (aExt["kind"] != bExt["kind"])
	{
		return false;
	}

	return aExt["uri"] == bExt["uri"];
}

static json reduceRtcpFeedback(const json& codecA, const json& codecB)
{
	MSC_TRACE();

	const auto rtcpFeedbackAIt = codecA.find("rtcpFeedback");
	const auto rtcpFeedbackBIt = codecB.find("rtcpFeedback");

	auto reducedRtcpFeedback = json::array();

	for (const auto& aFb : *rtcpFeedbackAIt)
	{
		const auto rtcpFeedbackIt = std::find_if(
		  rtcpFeedbackBIt->begin(),
		  rtcpFeedbackBIt->end(),
		  [&aFb](const json& bFb) -> bool
		  {
			  return (aFb["type"] == bFb["type"] && aFb["parameter"] == bFb["parameter"]);
		  });

		if (rtcpFeedbackIt != rtcpFeedbackBIt->end())
		{
			reducedRtcpFeedback.push_back(*rtcpFeedbackIt);
		}
	}

	return reducedRtcpFeedback;
}

static uint8_t getH264PacketizationMode(const json& codec)
{
	MSC_TRACE();

	const auto& parameters         = codec["parameters"];
	const auto packetizationModeIt = parameters.find("packetization-mode");

	// clang-format off
	if (
		packetizationModeIt == parameters.end() ||
		!packetizationModeIt->is_number_integer()
	)
	// clang-format on
	{
		return 0;
	}

	return packetizationModeIt->get<uint8_t>();
}

static uint8_t getH264LevelAssimetryAllowed(const json& codec)
{
	MSC_TRACE();

	const auto& parameters             = codec["parameters"];
	const auto levelAssimetryAllowedIt = parameters.find("level-asymmetry-allowed");

	// clang-format off
	if (
		levelAssimetryAllowedIt == parameters.end() ||
		!levelAssimetryAllowedIt->is_number_integer()
	)
	// clang-format on
	{
		return 0;
	}

	return levelAssimetryAllowedIt->get<uint8_t>();
}

static std::string getH264ProfileLevelId(const json& codec)
{
	MSC_TRACE();

	const auto& parameters      = codec["parameters"];
	const auto profileLevelIdIt = parameters.find("profile-level-id");

	if (profileLevelIdIt == parameters.end())
	{
		return "";
	}
	else if (profileLevelIdIt->is_number())
	{
		return std::to_string(profileLevelIdIt->get<int32_t>());
	}
	else
	{
		return profileLevelIdIt->get<std::string>();
	}
}

static std::string getVP9ProfileId(const json& codec)
{
	MSC_TRACE();

	const auto& parameters = codec["parameters"];
	const auto profileIdIt = parameters.find("profile-id");

	if (profileIdIt == parameters.end())
	{
		return "0";
	}

	if (profileIdIt->is_number())
	{
		return std::to_string(profileIdIt->get<int32_t>());
	}
	else
	{
		return profileIdIt->get<std::string>();
	}
}
