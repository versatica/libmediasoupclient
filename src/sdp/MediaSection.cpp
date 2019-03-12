#define MSC_CLASS "Sdp::MediaSection"
// #define MSC_LOG_DEV

#include "sdp/MediaSection.hpp"
#include "Logger.hpp"
#include <algorithm> // ::transform
#include <cctype>    // ::tolower
#include <sstream>
#include <utility>

using json = nlohmann::json;

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
	  const nlohmann::json& offerRtpParameters,
	  nlohmann::json& answerRtpParameters,
	  const nlohmann::json& codecOptions)
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

			this->mediaObject["rtp"].push_back(rtp);

			json& codecParameters = codec["parameters"];

			if (!codecOptions.empty())
			{
				auto offerCodecs = offerRtpParameters["codecs"];
				it = find_if(offerCodecs.begin(), offerCodecs.end(), [&codec](const json& offerCodec) {
					return offerCodec["payloadType"] == codec["payloadType"];
				});

				auto offerCodec = *it;
				auto mimeType   = codec["mimeType"].get<std::string>();
				std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

				if (mimeType == "audio/opus")
				{
					auto it2 = codecOptions.find("opusStereo");
					if (it2 != codecOptions.end())
					{
						auto opusStereo                          = (*it).get<uint8_t>();
						offerCodec["parameters"]["sprop-stereo"] = opusStereo != 0u ? 1 : 0;
						codecParameters["stereo"]                = opusStereo != 0u ? 1 : 0;
					}

					it2 = codecOptions.find("opusFec");
					if (it2 != codecOptions.end())
					{
						auto opusFec                             = (*it).get<uint8_t>();
						offerCodec["parameters"]["useinbandfec"] = opusFec != 0u ? 1 : 0;
						codecParameters["useinbandfec"]          = opusFec != 0u ? 1 : 0;
					}

					it2 = codecOptions.find("opusDtx");
					if (it2 != codecOptions.end())
					{
						auto opusDtx                       = (*it).get<uint8_t>();
						offerCodec["parameters"]["usedtx"] = opusDtx != 0u ? 1 : 0;
						codecParameters["usedtx"]          = opusDtx != 0u ? 1 : 0;
					}

					it2 = codecOptions.find("opusMaxPlaybackRate");
					if (it2 != codecOptions.end())
					{
						auto opusMaxPlaybackRate                    = (*it).get<uint32_t>();
						offerCodec["parameters"]["maxplaybackrate"] = opusMaxPlaybackRate;
					}
				}
				else if (mimeType == "video/vp8" || mimeType == "video/vp9" || mimeType == "video/h264" || mimeType == "video/h265")
				{
					auto it2 = codecOptions.find("videoGoogleStartBitrate");
					if (it2 != codecOptions.end())
					{
						auto startBitrate                                  = (*it).get<uint32_t>();
						offerCodec["parameters"]["x-google-start-bitrate"] = startBitrate;
					}

					it2 = codecOptions.find("videoGoogleMaxBitrate");
					if (it2 != codecOptions.end())
					{
						auto maxBitrate                                  = (*it).get<uint32_t>();
						offerCodec["parameters"]["x-google-max-bitrate"] = maxBitrate;
					}

					it2 = codecOptions.find("videoGoogleMinBitrate");
					if (it2 != codecOptions.end())
					{
						auto minBitrate                                  = (*it).get<uint32_t>();
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

			it = codec.find("rtcpFeedback");
			if (it != codec.end())
			{
				auto rtcpFeedback = *it;
				for (auto fb : rtcpFeedback)
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

		for (auto ext : answerRtpParameters["headerExtensions"])
		{
			// Don't add a header extension if not present in the offer.
			auto it = offerMediaObject.find("ext");
			if (it == offerMediaObject.end())
				continue;

			auto localExts = *it;
			it             = find_if(localExts.begin(), localExts.end(), [&ext](const json& localExt) {
        return localExt["uri"] == ext["uri"];
      });

			if (it == localExts.end())
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

			this->mediaObject["rtp"].push_back(rtp);

			it = codec.find("parameters");
			if (it != codec.end())
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

			it = codec.find("rtcpFeedback");
			if (it != codec.end())
			{
				auto rtcpFeedback = *it;
				for (auto fb : rtcpFeedback)
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

			for (auto ext : offerRtpParameters["headerExtensions"])
			{
				// Ignore MID and RID RTP extensions when receiving.
				if (
				  ext["uri"].get<std::string>() == "urn:ietf:params:rtp-hdrext:sdes:mid" ||
				  ext["uri"].get<std::string>() == "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id")
				{
					continue;
				}

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

			it = encoding.find("rtx");
			if ((it != encoding.end()) && ((*it).find("ssrc") != (*it).end()))
				rtxSsrc = encoding["rtx"]["ssrc"].get<uint32_t>();
			else
				rtxSsrc = 0u;

			this->mediaObject["ssrcs"]      = json::array();
			this->mediaObject["ssrcGroups"] = json::array();

			it = offerRtpParameters["rtcp"].find("cname");
			if (it != offerRtpParameters["rtcp"].end())
			{
				auto cname = (*it).get<std::string>();

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
