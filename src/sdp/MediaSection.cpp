#define MSC_CLASS "Sdp::MediaSection"
// #define MSC_LOG_DEV

#include "sdp/MediaSection.hpp"
#include "Logger.hpp"
#include <algorithm> // ::transform
#include <cctype>    // ::tolower
#include <regex>
#include <sstream>
#include <utility>

using json = nlohmann::json;

static std::string getCodecName(const json& codec)
{
	static const std::regex regex("^.*/", std::regex_constants::ECMAScript);

	auto mimeType = codec["mimeType"].get<std::string>();

	return std::regex_replace(mimeType, regex, "");
}

namespace mediasoupclient
{
namespace Sdp
{
	MediaSection::MediaSection(const nlohmann::json& iceParameters, const nlohmann::json& iceCandidates)
	{
		MSC_TRACE();

		// Set ICE parameters.
		this->SetIceParameters(iceParameters);

		// Set ICE candidates.
		this->mediaObject["candidates"] = json::array();

		for (auto& candidate : iceCandidates)
		{
			auto candidateObject = json::object();

			// mediasoup does mandate rtcp-mux so candidates component is always
			// RTP (1).
			candidateObject["component"]  = 1;
			candidateObject["foundation"] = candidate["foundation"];
			candidateObject["ip"]         = candidate["ip"];
			candidateObject["port"]       = candidate["port"];
			candidateObject["priority"]   = candidate["priority"];
			candidateObject["transport"]  = candidate["protocol"];
			candidateObject["type"]       = candidate["type"];

			if (candidate.find("tcpType") != candidate.end())
				candidateObject["tcptype"] = candidate["tcpType"];

			this->mediaObject["candidates"].push_back(candidateObject);
		}

		this->mediaObject["endOfCandidates"] = "end-of-candidates";
		this->mediaObject["iceOptions"]      = "renomination";
	}

	AnswerMediaSection::AnswerMediaSection(
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const nlohmann::json& offerMediaObject,
	  nlohmann::json& offerRtpParameters,
	  nlohmann::json& answerRtpParameters,
	  const nlohmann::json* codecOptions)
	  : MediaSection(iceParameters, iceCandidates)
	{
		MSC_TRACE();

		this->mediaObject["mid"]        = offerMediaObject["mid"].get<std::string>();
		this->mediaObject["type"]       = offerMediaObject["type"].get<std::string>();
		this->mediaObject["connection"] = { { "ip", "127.0.0.1" }, { "version", 4 } };
		this->mediaObject["protocol"]   = "RTP/SAVPF";
		this->mediaObject["port"]       = 7;
		this->mediaObject["direction"]  = "recvonly";

		this->mediaObject["rtp"]    = json::array();
		this->mediaObject["rtcpFb"] = json::array();
		this->mediaObject["fmtp"]   = json::array();

		for (auto& codec : answerRtpParameters["codecs"])
		{
			/* clang-format off */
			json rtp =
			{
				{ "payload", codec["payloadType"] },
				{ "codec",   getCodecName(codec)  },
				{ "rate",    codec["clockRate"]   }
			};
			/* clang-format on */

			auto jsonChannelsIt = codec.find("channels");
			if (jsonChannelsIt != codec.end() && jsonChannelsIt->is_number_unsigned())
			{
				auto channels = jsonChannelsIt->get<uint8_t>();
				if (channels > 1)
					rtp["encoding"] = channels;
			}

			this->mediaObject["rtp"].push_back(rtp);

			json& codecParameters = codec["parameters"];

			if (codecOptions != nullptr && !codecOptions->empty())
			{
				auto& offerCodecs = offerRtpParameters["codecs"];
				auto jsonCodecIt =
				  find_if(offerCodecs.begin(), offerCodecs.end(), [&codec](json& offerCodec) {
					  return offerCodec["payloadType"] == codec["payloadType"];
				  });

				auto& offerCodec = *jsonCodecIt;
				auto mimeType    = codec["mimeType"].get<std::string>();
				std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

				if (mimeType == "audio/opus")
				{
					auto jsonOpusStereoIt = codecOptions->find("opusStereo");
					if (jsonOpusStereoIt != codecOptions->end() && jsonOpusStereoIt->is_boolean())
					{
						auto opusStereo                          = jsonOpusStereoIt->get<bool>();
						offerCodec["parameters"]["sprop-stereo"] = opusStereo == true ? 1 : 0;
						codecParameters["stereo"]                = opusStereo == true ? 1 : 0;
					}

					auto jsonOpusFec = codecOptions->find("opusFec");
					if (jsonOpusFec != codecOptions->end() && jsonOpusFec->is_boolean())
					{
						auto opusFec                             = jsonOpusFec->get<bool>();
						offerCodec["parameters"]["useinbandfec"] = opusFec == true ? 1 : 0;
						codecParameters["useinbandfec"]          = opusFec == true ? 1 : 0;
					}

					auto jsonOpusDtx = codecOptions->find("opusDtx");
					if (jsonOpusDtx != codecOptions->end() && jsonOpusDtx->is_boolean())
					{
						auto opusDtx                       = jsonOpusDtx->get<bool>();
						offerCodec["parameters"]["usedtx"] = opusDtx == true ? 1 : 0;
						codecParameters["usedtx"]          = opusDtx == true ? 1 : 0;
					}

					auto jsonOpusMaxPlaybackRate = codecOptions->find("opusMaxPlaybackRate");
					if (jsonOpusMaxPlaybackRate != codecOptions->end() && jsonOpusMaxPlaybackRate->is_number_unsigned())
					{
						auto opusMaxPlaybackRate                    = jsonOpusMaxPlaybackRate->get<uint32_t>();
						offerCodec["parameters"]["maxplaybackrate"] = opusMaxPlaybackRate;
					}
				}
				else if (mimeType == "video/vp8" || mimeType == "video/vp9" || mimeType == "video/h264" || mimeType == "video/h265")
				{
					auto jsonVideoGoogleStartBitrate = codecOptions->find("videoGoogleStartBitrate");
					if (jsonVideoGoogleStartBitrate != codecOptions->end() && jsonVideoGoogleStartBitrate->is_number_unsigned())
					{
						auto startBitrate = jsonVideoGoogleStartBitrate->get<uint32_t>();
						offerCodec["parameters"]["x-google-start-bitrate"] = startBitrate;
					}

					auto jsonVideoGoogleMaxBitrate = codecOptions->find("videoGoogleMaxBitrate");
					if (jsonVideoGoogleMaxBitrate != codecOptions->end() && jsonVideoGoogleMaxBitrate->is_number_unsigned())
					{
						auto maxBitrate = jsonVideoGoogleMaxBitrate->get<uint32_t>();
						offerCodec["parameters"]["x-google-max-bitrate"] = maxBitrate;
					}

					auto jsonVideoGoogleMinBitrate = codecOptions->find("videoGoogleMinBitrate");
					if (jsonVideoGoogleMinBitrate != codecOptions->end() && jsonVideoGoogleMinBitrate->is_number_unsigned())
					{
						auto minBitrate = jsonVideoGoogleMinBitrate->get<uint32_t>();
						offerCodec["parameters"]["x-google-min-bitrate"] = minBitrate;
					}
				}
			}

			/* clang-format off */
			json fmtp =
			{
				{ "payload", codec["payloadType"] }
			};
			/* clang-format on */

			std::ostringstream config;
			for (auto& item : codecParameters.items())
			{
				if (!config.str().empty())
					config << ";";

				config << item.key();
				config << "=";
				if (item.value().is_string())
					config << item.value().get<std::string>();
				else if (item.value().is_number_float())
					config << item.value().get<float>();
				else if (item.value().is_number())
					config << item.value().get<int>();
			}

			if (!config.str().empty())
			{
				fmtp["config"] = config.str();
				this->mediaObject["fmtp"].push_back(fmtp);
			}

			auto jsonRtcpFeedbackIt = codec.find("rtcpFeedback");
			if (jsonRtcpFeedbackIt != codec.end())
			{
				auto rtcpFeedback = *jsonRtcpFeedbackIt;
				for (auto& fb : rtcpFeedback)
				{
					/* clang-format off */
					this->mediaObject["rtcpFb"].push_back(
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

		for (auto& codec : answerRtpParameters["codecs"])
		{
			auto payloadType = codec["payloadType"].get<uint8_t>();

			if (!payloads.empty())
				payloads.append(" ");

			payloads.append(std::to_string(payloadType));
		}

		this->mediaObject["payloads"] = payloads;
		this->mediaObject["ext"]      = json::array();

		for (auto& ext : answerRtpParameters["headerExtensions"])
		{
			// Don't add a header extension if not present in the offer.
			auto jsonExtsIt = offerMediaObject.find("ext");
			if (jsonExtsIt == offerMediaObject.end())
				continue;

			auto localExts  = *jsonExtsIt;
			auto localExtIt = find_if(localExts.begin(), localExts.end(), [&ext](const json& localExt) {
				auto jsonUriIt = localExt.find("uri");
				if (jsonUriIt == localExt.end() || !jsonUriIt->is_string())
					return false;

				jsonUriIt = ext.find("uri");
				if (jsonUriIt == ext.end() || !jsonUriIt->is_string())
					return false;

				return localExt["uri"] == ext["uri"];
			});

			if (localExtIt == localExts.end())
				continue;

			/* clang-format off */
			this->mediaObject["ext"].push_back(
				{
					{ "uri",   ext["uri"] },
					{ "value", ext["id"]  }
				});
			/* clang-format on */
		}

		this->mediaObject["rtcpMux"]   = "rtcp-mux";
		this->mediaObject["rtcpRsize"] = "rtcp-rsize";

		// Set DTLS role.
		this->SetDtlsRole(dtlsParameters["role"].get<std::string>());
	}

	OfferMediaSection::OfferMediaSection(
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const std::string& mid,
	  const std::string& kind,
	  const nlohmann::json& offerRtpParameters,
	  const std::string& streamId,
	  const std::string& trackId)
	  : MediaSection(iceParameters, iceCandidates)
	{
		MSC_TRACE();

		this->mediaObject["mid"]  = mid;
		this->mediaObject["type"] = kind;

		this->mediaObject["connection"] = { { "ip", "127.0.0.1" }, { "version", 4 } };
		this->mediaObject["protocol"]   = "UDP/TLS/RTP/SAVPF";
		this->mediaObject["port"]       = 7;
		this->mediaObject["direction"]  = "sendonly";

		this->mediaObject["rtp"]    = json::array();
		this->mediaObject["rtcpFb"] = json::array();
		this->mediaObject["fmtp"]   = json::array();

		for (auto& codec : offerRtpParameters["codecs"])
		{
			/* clang-format off */
			json rtp =
			{
				{ "payload", codec["payloadType"] },
				{ "codec",   getCodecName(codec)  },
				{ "rate",    codec["clockRate"]   }
			};
			/* clang-format on */

			auto jsonChannelsIt = codec.find("channels");
			if (jsonChannelsIt != codec.end() && jsonChannelsIt->is_number_unsigned())
			{
				auto channels = (*jsonChannelsIt).get<uint8_t>();
				if (channels > 1)
					rtp["encoding"] = channels;
			}

			this->mediaObject["rtp"].push_back(rtp);

			if (codec.find("parameters") != codec.end())
			{
				const json& codecParameters = codec["parameters"];

				/* clang-format off */
				json fmtp =
				{
					{ "payload", codec["payloadType"] }
				};
				/* clang-format on */

				std::ostringstream config;
				for (auto& item : codecParameters.items())
				{
					if (!config.str().empty())
						config << ";";

					config << item.key();
					config << "=";
					if (item.value().is_string())
						config << item.value().get<std::string>();
					else if (item.value().is_number_float())
						config << item.value().get<float>();
					else if (item.value().is_number())
						config << item.value().get<int>();
				}

				if (!config.str().empty())
				{
					fmtp["config"] = config.str();
					this->mediaObject["fmtp"].push_back(fmtp);
				}
			}

			auto jsonRtcpFeedbackIt = codec.find("rtcpFeedback");
			if (codec.find("rtcpFeedback") != codec.end())
			{
				auto rtcpFeedback = *jsonRtcpFeedbackIt;
				for (auto& fb : rtcpFeedback)
				{
					/* clang-format off */
					this->mediaObject["rtcpFb"].push_back(
						{
							{ "payload", codec["payloadType"] },
							{ "type",    fb["type"]           },
							{ "subtype", fb["parameter"]      }
						});
					/* clang-format on */
				}
			}

			std::string payloads;

			for (auto& codec : offerRtpParameters["codecs"])
			{
				auto payloadType = codec["payloadType"].get<uint8_t>();

				if (!payloads.empty())
					payloads.append(" ");

				payloads.append(std::to_string(payloadType));
			}

			this->mediaObject["payloads"] = payloads;

			this->mediaObject["ext"] = json::array();

			for (auto& ext : offerRtpParameters["headerExtensions"])
			{
				/* clang-format off */
				this->mediaObject["ext"].push_back(
					{
						{ "uri",   ext["uri"] },
						{ "value", ext["id"]  }
					});
				/* clang-format on */
			}

			this->mediaObject["rtcpMux"]   = "rtcp-mux";
			this->mediaObject["rtcpRsize"] = "rtcp-rsize";

			auto encoding = offerRtpParameters["encodings"][0];
			auto ssrc     = encoding["ssrc"].get<uint32_t>();
			uint32_t rtxSsrc;

			auto jsonRtxIt = encoding.find("rtx");
			if ((jsonRtxIt != encoding.end()) && ((*jsonRtxIt).find("ssrc") != (*jsonRtxIt).end()))
				rtxSsrc = encoding["rtx"]["ssrc"].get<uint32_t>();
			else
				rtxSsrc = 0u;

			this->mediaObject["ssrcs"]      = json::array();
			this->mediaObject["ssrcGroups"] = json::array();

			auto jsonCnameIt = offerRtpParameters["rtcp"].find("cname");
			if (jsonCnameIt != offerRtpParameters["rtcp"].end() && jsonCnameIt->is_string())
			{
				auto cname = (*jsonCnameIt).get<std::string>();

				std::string msid(streamId);
				msid.append(" ").append(trackId);

				this->mediaObject["ssrcs"].push_back(
				  { { "id", ssrc }, { "attribute", "cname" }, { "value", cname } });

				this->mediaObject["ssrcs"].push_back(
				  { { "id", ssrc }, { "attribute", "msid" }, { "value", msid } });

				if (rtxSsrc != 0u)
				{
					std::string ssrcs = std::to_string(ssrc).append(" ").append(std::to_string(rtxSsrc));

					this->mediaObject["ssrcs"].push_back(
					  { { "id", rtxSsrc }, { "attribute", "cname" }, { "value", cname } });

					this->mediaObject["ssrcs"].push_back(
					  { { "id", rtxSsrc }, { "attribute", "msid" }, { "value", msid } });

					// Associate original and retransmission SSRCs.
					this->mediaObject["ssrcGroups"].push_back({ { "semantics", "FID" }, { "ssrcs", ssrcs } });
				}
			}
		}

		// Set DTLS role.
		this->SetDtlsRole(dtlsParameters["role"].get<std::string>());
	}
} // namespace Sdp
} // namespace mediasoupclient
