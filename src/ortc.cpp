#define MSC_CLASS "ortc"

#include "ortc.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include <media/base/h264_profile_level_id.h>
#include <algorithm> // std::find_if
#include <regex>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
using namespace mediasoupclient;

static constexpr uint32_t ProbatorSsrc{ 1234u };

// Static functions declaration.
static bool isRtxCodec(const json& codec);
static bool matchCodecs(json& aCodec, const json& bCodec, bool strict = false, bool modify = false);
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

			// channels is optional. If unset, set it to 1 (just if audio).
			if (codec["kind"] == "audio")
			{
				if (channelsIt == codec.end() || !channelsIt->is_number_integer())
					codec["channels"] = 1;
			}
			else
			{
				if (channelsIt != codec.end())
					codec.erase("channels");
			}

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

				if (!value.is_string() && !value.is_number() && value != nullptr)
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
			{
				MSC_THROW_TYPE_ERROR("params.mid is not a string");
			}

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

			// Retrieve media kind from mimeType.
			auto kind = mimeTypeMatch[1].str();

			// channels is optional. If unset, set it to 1 (just for audio).
			if (kind == "audio")
			{
				if (channelsIt == codec.end() || !channelsIt->is_number_integer())
					codec["channels"] = 1;
			}
			else
			{
				if (channelsIt != codec.end())
					codec.erase("channels");
			}

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

				if (!value.is_string() && !value.is_number() && value != nullptr)
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
			{
				MSC_THROW_TYPE_ERROR("missing ext.uri");
			}

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
			if (cnameIt != rtcp.end() && !cnameIt->is_string())
				MSC_THROW_TYPE_ERROR("invalid rtcp.cname");

			// reducedSize is optional. If unset set it to true.
			if (reducedSizeIt == rtcp.end() || !reducedSizeIt->is_boolean())
				rtcp["reducedSize"] = true;
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
				MSC_THROW_TYPE_ERROR("caps is not an object");

			auto numStreamsIt = caps.find("numStreams");

			// numStreams is mandatory.
			if (numStreamsIt == caps.end() || !numStreamsIt->is_object())
				MSC_THROW_TYPE_ERROR("missing caps.numStreams");

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
				MSC_THROW_TYPE_ERROR("numStreams is not an object");

			auto osIt  = numStreams.find("OS");
			auto misIt = numStreams.find("MIS");

			// OS is mandatory.
			if (osIt == numStreams.end() || !osIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing numStreams.OS");

			// MIS is mandatory.
			if (misIt == numStreams.end() || !misIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing numStreams.MIS");
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
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto portIt           = params.find("port");
			auto osIt             = params.find("OS");
			auto misIt            = params.find("MIS");
			auto maxMessageSizeIt = params.find("maxMessageSize");

			// port is mandatory.
			if (portIt == params.end() || !portIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing params.port");

			// OS is mandatory.
			if (osIt == params.end() || !osIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing params.OS");

			// MIS is mandatory.
			if (misIt == params.end() || !misIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing params.MIS");

			// maxMessageSize is mandatory.
			if (maxMessageSizeIt == params.end() || !maxMessageSizeIt->is_number_integer())
			{
				MSC_THROW_TYPE_ERROR("missing params.maxMessageSize");
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
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto streamIdIt          = params.find("streamId");
			auto orderedIt           = params.find("ordered");
			auto maxPacketLifeTimeIt = params.find("maxPacketLifeTime");
			auto maxRetransmitsIt    = params.find("maxRetransmits");
			auto priorityIt          = params.find("priority");
			auto labelIt             = params.find("label");
			auto protocolIt          = params.find("protocol");

			// streamId is mandatory.
			if (streamIdIt == params.end() || !streamIdIt->is_number_integer())
				MSC_THROW_TYPE_ERROR("missing params.streamId");

			// ordered is optional.
			bool orderedGiven = false;

			if (orderedIt != params.end() && orderedIt->is_boolean())
				orderedGiven = true;
			else
				params["ordered"] = true;

			// maxPacketLifeTime is optional. If unset set it to 0.
			if (maxPacketLifeTimeIt == params.end() || !maxPacketLifeTimeIt->is_number_integer())
			{
				params["maxPacketLifeTime"] = 0u;
			}

			// maxRetransmits is optional. If unset set it to 0.
			if (maxRetransmitsIt == params.end() || !maxRetransmitsIt->is_number_integer())
			{
				params["maxRetransmits"] = 0u;
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

			// priority is optional. If unset set it to empty string.
			if (priorityIt == params.end() || !priorityIt->is_string())
				params["priority"] = "";

			// label is optional. If unset set it to empty string.
			if (labelIt == params.end() || !labelIt->is_string())
				params["label"] = "";

			// protocol is optional. If unset set it to empty string.
			if (protocolIt == params.end() || !protocolIt->is_string())
				params["protocol"] = "";
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
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto usernameFragmentIt = params.find("usernameFragment");
			auto passwordIt         = params.find("password");
			auto iceLiteIt          = params.find("iceLite");

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
				params["iceLite"] = false;
		}

		/**
		 * Validates IceCandidate. It may modify given data by adding missing
		 * fields with default values.
		 * It throws if invalid.
		 */
		void validateIceCandidate(json& params)
		{
			MSC_TRACE();

			static const std::regex ProtocolRegex(
			  "(udp|tcp)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			static const std::regex TypeRegex(
			  "(host|srflx|prflx|relay)", std::regex_constants::ECMAScript | std::regex_constants::icase);

			if (!params.is_object())
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto foundationIt = params.find("foundation");
			auto priorityIt   = params.find("priority");
			auto ipIt         = params.find("ip");
			auto protocolIt   = params.find("protocol");
			auto portIt       = params.find("port");
			auto typeIt       = params.find("type");

			// foundation is mandatory.
			if (
			  foundationIt == params.end() ||
			  (!foundationIt->is_string() || foundationIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.foundation");
			}

			// priority is mandatory.
			if (priorityIt == params.end() || !priorityIt->is_number_unsigned())
				MSC_THROW_TYPE_ERROR("missing params.priority");

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
			std::regex_match(protocolIt->get<std::string>(), protocolMatch, ProtocolRegex);

			if (protocolMatch.empty())
				MSC_THROW_TYPE_ERROR("invalid params.protocol");

			// port is mandatory.
			if (portIt == params.end() || !portIt->is_number_unsigned())
				MSC_THROW_TYPE_ERROR("missing params.port");

			// type is mandatory.
			if (typeIt == params.end() || (!typeIt->is_string() || typeIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.type");
			}

			std::smatch typeMatch;
			std::regex_match(typeIt->get<std::string>(), typeMatch, TypeRegex);

			if (typeMatch.empty())
				MSC_THROW_TYPE_ERROR("invalid params.type");
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
				MSC_THROW_TYPE_ERROR("params is not an array");

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
		void validateDtlsFingerprint(json& params)
		{
			MSC_TRACE();

			if (!params.is_object())
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto algorithmIt = params.find("algorithm");
			auto valueIt     = params.find("value");

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
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto roleIt         = params.find("role");
			auto fingerprintsIt = params.find("fingerprints");

			// role is mandatory.
			if (roleIt == params.end() || (!roleIt->is_string() || roleIt->get<std::string>().empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.role");
			}

			std::smatch roleMatch;
			std::regex_match(roleIt->get<std::string>(), roleMatch, RoleRegex);

			if (roleMatch.empty())
				MSC_THROW_TYPE_ERROR("invalid params.role");

			// fingerprints is mandatory.
			if (fingerprintsIt == params.end() || (!fingerprintsIt->is_array() || fingerprintsIt->empty()))
			{
				MSC_THROW_TYPE_ERROR("missing params.fingerprints");
			}

			for (auto& fingerprint : *fingerprintsIt)
				validateDtlsFingerprint(fingerprint);
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
				MSC_THROW_TYPE_ERROR("params is not an object");

			auto opusStereoIt              = params.find("opusStereo");
			auto opusFecIt                 = params.find("opusFec");
			auto opusDtxIt                 = params.find("opusDtx");
			auto opusMaxPlaybackRateIt     = params.find("opusMaxPlaybackRate");
			auto opusPtimeIt               = params.find("opusPtime");
			auto videoGoogleStartBitrateIt = params.find("videoGoogleStartBitrate");
			auto videoGoogleMaxBitrateIt   = params.find("videoGoogleMaxBitrate");
			auto videoGoogleMinBitrateIt   = params.find("videoGoogleMinBitrate");

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

			if (opusMaxPlaybackRateIt != params.end() && !opusMaxPlaybackRateIt->is_number_unsigned())
			{
				MSC_THROW_TYPE_ERROR("invalid params.opusMaxPlaybackRate");
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
				{ "headerExtensions", json::array() }
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
					extendedCodec["channels"] = matchingLocalCodec["channels"];

				extendedRtpCapabilities["codecs"].push_back(extendedCodec);
			}

			// Match RTX codecs.
			json& extendedCodecs = extendedRtpCapabilities["codecs"];

			for (json& extendedCodec : extendedCodecs)
			{
				auto& localCodecs = localCaps["codecs"];
				auto localCodecIt = std::find_if(
				  localCodecs.begin(), localCodecs.end(), [&extendedCodec](const json& localCodec) {
					  return isRtxCodec(localCodec) &&
					         localCodec["parameters"]["apt"] == extendedCodec["localPayloadType"];
				  });

				if (localCodecIt == localCodecs.end())
					continue;

				auto& matchingLocalRtxCodec = *localCodecIt;
				auto& remoteCodecs          = remoteCaps["codecs"];
				auto remoteCodecIt          = std::find_if(
          remoteCodecs.begin(), remoteCodecs.end(), [&extendedCodec](const json& remoteCodec) {
            return isRtxCodec(remoteCodec) &&
                   remoteCodec["parameters"]["apt"] == extendedCodec["remotePayloadType"];
          });

				if (remoteCodecIt == remoteCodecs.end())
					continue;

				auto& matchingRemoteRtxCodec = *remoteCodecIt;

				extendedCodec["localRtxPayloadType"]  = matchingLocalRtxCodec["preferredPayloadType"];
				extendedCodec["remoteRtxPayloadType"] = matchingRemoteRtxCodec["preferredPayloadType"];
			}

			// Match header extensions.
			auto& remoteExts = remoteCaps["headerExtensions"];

			for (auto& remoteExt : remoteExts)
			{
				auto& localExts = localCaps["headerExtensions"];
				auto localExtIt =
				  std::find_if(localExts.begin(), localExts.end(), [&remoteExt](const json& localExt) {
					  return matchHeaderExtensions(localExt, remoteExt);
				  });

				if (localExtIt == localExts.end())
					continue;

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
				{ "headerExtensions", json::array() }
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
					{ "parameters",           extendedCodec["localParameters"]   },
					{ "rtcpFeedback",         extendedCodec["rtcpFeedback"]      },
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
					codec["channels"] = extendedCodec["channels"];

				rtpCapabilities["codecs"].push_back(codec);

				// Add RTX codec.
				if (extendedCodec["remoteRtxPayloadType"] == nullptr)
					continue;

				auto mimeType = extendedCodec["kind"].get<std::string>().append("/rtx");

				// clang-format off
				json rtxCodec =
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

			return rtpCapabilities;
		}

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
					{ "parameters",   extendedCodec["localParameters"]  },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
					codec["channels"] = extendedCodec["channels"];

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
					{ "parameters",   extendedCodec["remoteParameters"] },
					{ "rtcpFeedback", extendedCodec["rtcpFeedback"]     }
				};
				// clang-format on

				if (extendedCodec.contains("channels"))
					codec["channels"] = extendedCodec["channels"];

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
					{ "uri",        extendedExtension["uri"]     },
					{ "id",         extendedExtension["sendId"]  },
					{ "encrypt",    extendedExtension["encrypt"] },
					{ "parameters", json::object()               }
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
							it = rtcpFeedback.erase(it);
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
		}

		/**
		 * Create RTP parameters for a Consumer for the RTP probator.
		 */
		const json generateProbatorRtpParameters(const json& videoRtpParameters)
		{
			MSC_TRACE();

			// This may throw.
			json validatedRtpParameters = videoRtpParameters;

			// This may throw.
			ortc::validateRtpParameters(validatedRtpParameters);

			// clang-format off
			json rtpParameters =
			{
				{ "mid",              nullptr        },
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

			auto& codecs = extendedRtpCapabilities["codecs"];
			auto codecIt = std::find_if(codecs.begin(), codecs.end(), [&kind](const json& codec) {
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
				return false;

			auto& firstMediaCodec = rtpParameters["codecs"][0];
			auto& codecs          = extendedRtpCapabilities["codecs"];
			auto codecIt =
			  std::find_if(codecs.begin(), codecs.end(), [&firstMediaCodec](const json& codec) {
				  return codec["remotePayloadType"] == firstMediaCodec["payloadType"];
			  });

			return codecIt != codecs.end();
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
	auto bMimeTypeIt = bCodec.find("mimeType");
	auto aMimeType   = aMimeTypeIt->get<std::string>();
	auto bMimeType   = bMimeTypeIt->get<std::string>();

	std::transform(aMimeType.begin(), aMimeType.end(), aMimeType.begin(), ::tolower);
	std::transform(bMimeType.begin(), bMimeType.end(), bMimeType.begin(), ::tolower);

	if (aMimeType != bMimeType)
		return false;

	if (aCodec["clockRate"] != bCodec["clockRate"])
		return false;

	if (aCodec.contains("channels") != bCodec.contains("channels"))
		return false;

	if (aCodec.contains("channels") && aCodec["channels"] != bCodec["channels"])
		return false;

	// Match H264 parameters.
	if (aMimeType == "video/h264")
	{
		auto aPacketizationMode = getH264PacketizationMode(aCodec);
		auto bPacketizationMode = getH264PacketizationMode(bCodec);

		if (aPacketizationMode != bPacketizationMode)
			return false;

		// If strict matching check profile-level-id.
		if (strict)
		{
			webrtc::H264::CodecParameterMap aParameters;
			webrtc::H264::CodecParameterMap bParameters;

			aParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(aCodec));
			aParameters["packetization-mode"]      = std::to_string(aPacketizationMode);
			aParameters["profile-level-id"]        = getH264ProfileLevelId(aCodec);
			bParameters["level-asymmetry-allowed"] = std::to_string(getH264LevelAssimetryAllowed(bCodec));
			bParameters["packetization-mode"]      = std::to_string(bPacketizationMode);
			bParameters["profile-level-id"]        = getH264ProfileLevelId(bCodec);

			if (!webrtc::H264::IsSameH264Profile(aParameters, bParameters))
				return false;

			webrtc::H264::CodecParameterMap newParameters;

			try
			{
				webrtc::H264::GenerateProfileLevelIdForAnswer(aParameters, bParameters, &newParameters);
			}
			catch (std::runtime_error)
			{
				return false;
			}

			if (modify)
			{
				auto profileLevelIdIt = newParameters.find("profile-level-id");

				if (profileLevelIdIt != newParameters.end())
					aCodec["parameters"]["profile-level-id"] = profileLevelIdIt->second;
				else
					aCodec["parameters"].erase("profile-level-id");
			}
		}
	}
	// Match VP9 parameters.
	else if (aMimeType == "video/vp9")
	{
		// If strict matching check profile-id.
		if (strict)
		{
			auto aProfileId = getVP9ProfileId(aCodec);
			auto bProfileId = getVP9ProfileId(bCodec);

			if (aProfileId != bProfileId)
				return false;
		}
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
	auto rtcpFeedbackAIt     = codecA.find("rtcpFeedback");
	auto rtcpFeedbackBIt     = codecB.find("rtcpFeedback");

	for (auto& aFb : *rtcpFeedbackAIt)
	{
		auto rtcpFeedbackIt =
		  std::find_if(rtcpFeedbackBIt->begin(), rtcpFeedbackBIt->end(), [&aFb](const json& bFb) {
			  return (aFb["type"] == bFb["type"] && aFb["parameter"] == bFb["parameter"]);
		  });

		if (rtcpFeedbackIt != rtcpFeedbackBIt->end())
			reducedRtcpFeedback.push_back(*rtcpFeedbackIt);
	}

	return reducedRtcpFeedback;
}

static uint8_t getH264PacketizationMode(const json& codec)
{
	MSC_TRACE();

	auto& parameters         = codec["parameters"];
	auto packetizationModeIt = parameters.find("packetization-mode");

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

	auto& parameters             = codec["parameters"];
	auto levelAssimetryAllowedIt = parameters.find("level-asymmetry-allowed");

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

	auto& parameters      = codec["parameters"];
	auto profileLevelIdIt = parameters.find("profile-level-id");

	if (profileLevelIdIt == parameters.end())
		return "";
	else if (profileLevelIdIt->is_number())
		return std::to_string(profileLevelIdIt->get<int32_t>());
	else
		return profileLevelIdIt->get<std::string>();
}

static std::string getVP9ProfileId(const json& codec)
{
	MSC_TRACE();

	auto& parameters = codec["parameters"];
	auto profileIdIt = parameters.find("profile-id");

	if (profileIdIt == parameters.end())
		return "0";

	if (profileIdIt->is_number())
		return std::to_string(profileIdIt->get<int32_t>());
	else
		return profileIdIt->get<std::string>();
}
