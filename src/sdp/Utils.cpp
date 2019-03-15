#define MSC_CLASS "Sdp::Utils"
// #define MSC_LOG_DEV

#include "sdp/Utils.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <algorithm> // ::transform
#include <cctype>    // ::tolower
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mediasoupclient
{
namespace Sdp
{
	namespace Utils
	{
		json extractRtpCapabilities(const json& sdpObj)
		{
			MSC_TRACE();

			// Map of RtpCodecParameters indexed by payload type.
			std::map<uint8_t, json> codecsMap;

			// Array of RtpHeaderExtensions.
			auto headerExtensions = json::array();

			// Whether a m=audio/video section has been already found.
			bool gotAudio = false;
			bool gotVideo = false;

			for (auto& m : sdpObj["media"])
			{
				auto kind = m["type"].get<std::string>();

				if (kind == "audio")
				{
					if (gotAudio)
						continue;

					gotAudio = true;
				}
				else if (kind == "video")
				{
					if (gotVideo)
						continue;

					gotVideo = true;
				}
				else
				{
					continue;
				}

				// Get codecs.
				for (auto& rtp : m["rtp"])
				{
					std::string mimeType(kind);
					mimeType.append("/").append(rtp["codec"].get<std::string>());

					/* clang-format off */
					json codec =
					{
						{ "name",                 rtp["codec"]   },
						{ "mimeType",             mimeType       },
						{ "kind",                 kind           },
						{ "clockRate",            rtp["rate"]    },
						{ "preferredPayloadType", rtp["payload"] },
						{ "rtcpFeedback",         json::array()  },
						{ "parameters",           json::object() }
					};
					/* clang-format on */

					if (kind == "audio")
					{
						auto it = rtp.find("encoding");
						if (it != rtp.end())
							codec["channels"] = std::stoi(it->get<std::string>());
						else
							codec["channels"] = 1;
					}

					codecsMap[codec["preferredPayloadType"].get<uint8_t>()] = codec;
				}

				// Get codec parameters.
				for (auto& fmtp : m["fmtp"])
				{
					auto parameters = sdptransform::parseParams(fmtp["config"]);

					auto it = codecsMap.find(fmtp["payload"]);
					if (it == codecsMap.end())
						continue;

					auto& codec = it->second;

					codec["parameters"] = parameters;
				}

				// Get RTCP feedback for each codec.
				for (auto& fb : m["rtcpFb"])
				{
					auto itCodec = codecsMap.find(std::stoi(fb["payload"].get<std::string>()));
					if (itCodec == codecsMap.end())
						continue;

					auto& codec = itCodec->second;

					/* clang-format off */
					json feedback =
					{
						{"type", fb["type"]}
					};
					/* clang-format on */

					auto it = fb.find("subtype");
					if (it != fb.end())
						feedback["parameter"] = *it;

					codec["rtcpFeedback"].push_back(feedback);
				}

				// Get RTP header extensions.
				for (auto& ext : m["ext"])
				{
					/* clang-format off */
					json headerExtension =
					{
							{ "kind",        kind },
							{ "uri",         ext["uri"] },
							{ "preferredId", ext["value"] }
					};
					/* clang-format on */

					headerExtensions.push_back(headerExtension);
				}
			}

			/* clang-format off */
			json rtpCapabilities =
			{
				{ "headerExtensions", headerExtensions },
				{ "codecs",           json::array() },
				{ "fecMechanisms",    json::array() } // TODO
			};
			/* clang-format on */

			for (auto& kv : codecsMap)
				rtpCapabilities["codecs"].push_back(kv.second);

			return rtpCapabilities;
		}

		json extractDtlsParameters(const json& sdpObj)
		{
			MSC_TRACE();

			json m, fingerprint;
			std::string role;

			for (auto& media : sdpObj["media"])
			{
				if (media.find("iceUfrag") != media.end() && media["port"] != 0)
				{
					m = media;
					break;
				}
			}

			if (m.find("fingerprint") != m.end())
				fingerprint = m["fingerprint"];
			else if (sdpObj.find("fingerprint") != sdpObj.end())
				fingerprint = sdpObj["fingerprint"];

			if (m.find("setup") != m.end())
			{
				std::string setup = m["setup"];

				if (setup == "active")
					role = "client";
				else if (setup == "passive")
					role = "server";
				else if (setup == "actpass")
					role = "auto";
			}

			// clang-format off
			json dtlsParameters =
			{
				{ "role",         role },
				{ "fingerprints",
					{
						{
							{ "algorithm", fingerprint["type"] },
							{ "value",     fingerprint["hash"] }
						}
					}
				}
			};
			// clang-format on

			return dtlsParameters;
		}

		void addLegacySimulcast(json& offerMediaObject, uint8_t numStreams)
		{
			MSC_TRACE();

			if (numStreams <= 1)
				return;

			// Get the SSRC.
			auto mSsrcs = offerMediaObject["ssrcs"];

			auto it = std::find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
				return line["attribute"].get<std::string>() == "msid";
			});

			if (it == mSsrcs.end())
				throw Exception("a=ssrc line with msid information not found");

			auto ssrcMsidLine = *it;

			auto v        = mediasoupclient::Utils::split(ssrcMsidLine["value"].get<std::string>(), ' ');
			auto streamId = v[0];
			auto trackId  = v[1];

			auto firstSsrc = ssrcMsidLine["id"].get<std::uint32_t>();
			uint32_t firstRtxSsrc{ 0 };

			// Get the SSRC for RTX.

			it = offerMediaObject.find("ssrcGroups");
			if (it != offerMediaObject.end())
			{
				auto ssrcGroups = *it;

				std::find_if(
				  ssrcGroups.begin(), ssrcGroups.end(), [&firstSsrc, &firstRtxSsrc](const json& line) {
					  if (line["semantics"].get<std::string>() != "FID")
						  return false;

					  auto v = mediasoupclient::Utils::split(line["ssrcs"].get<std::string>(), ' ');
					  if (std::stoull(v[0]) == firstSsrc)
					  {
						  firstRtxSsrc = std::stoull(v[1]);

						  return true;
					  }

					  return false;
				  });
			}

			it = std::find_if(mSsrcs.begin(), mSsrcs.end(), [&firstSsrc](const json& line) {
				return (
				  line["attribute"].get<std::string>() == "cname" && line["id"].get<uint32_t>() == firstSsrc);
			});

			if (it == mSsrcs.end())
				throw Exception("CNAME line not found");

			auto cname = (*it)["value"].get<std::string>();

			auto ssrcs    = json::array();
			auto rtxSsrcs = json::array();

			for (uint8_t i = 0; i < numStreams; ++i)
			{
				ssrcs.push_back(firstSsrc + i);

				if (firstRtxSsrc != 0u)
					rtxSsrcs.push_back(firstRtxSsrc + i);
			}

			offerMediaObject["ssrcGroups"] = json::array();
			offerMediaObject["ssrcs"]      = json::array();

			std::vector<uint32_t> ussrcs = ssrcs;
			auto ssrcsLine               = mediasoupclient::Utils::join(ussrcs, ' ');

			std::string msidValue(streamId);
			msidValue.append(" ").append(trackId);

			/* clang-format off */
			offerMediaObject["ssrcGroups"].push_back(
				{
					{ "semantics", "SIM"     },
					{ "ssrcs",     ssrcsLine },
				});

			for (auto &i : ssrcs)
			{
				auto ssrc = i.get<uint32_t>();

				offerMediaObject["ssrcs"].push_back(
					{
						{ "id",        ssrc    },
						{ "attribute", "cname" },
						{ "value",     cname   }
					});

				offerMediaObject["ssrcs"].push_back(
					{
						{ "id",        ssrc      },
						{ "attribute", "msid"    },
						{ "value",     msidValue }
					});
			}

			for (uint8_t i = 0; i < rtxSsrcs.size(); ++i)
			{
				auto ssrc = ssrcs[i].get<uint32_t>();
				auto rtxSsrc = rtxSsrcs[i].get<uint32_t>();

				std::string fid;
				fid = std::to_string(ssrc).append(" ");
				fid.append(std::to_string(rtxSsrc));

				offerMediaObject["ssrcGroups"].push_back(
					{
						{ "semantics", "FID" },
						{ "ssrcs",     fid   }
					});

				offerMediaObject["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc },
						{ "attribute", "cname" },
						{ "value",     cname   }
					});

				offerMediaObject["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc   },
						{ "attribute", "msid"    },
						{ "value",     msidValue }
					});
			}
			/* clang-format on */
		};

		std::string getCname(const json& offerMediaObject)
		{
			MSC_TRACE();

			const json& mSsrcs = offerMediaObject["ssrcs"];

			auto it = find_if(
			  mSsrcs.begin(), mSsrcs.end(), [](const json& line) { return line["attribute"] == "cname"; });

			if (it == mSsrcs.end())
				throw new std::runtime_error("a=ssrc line with cname information not found");

			auto ssrcCnameLine = *it;

			return ssrcCnameLine["value"].get<std::string>();
		}

		json getRtpEncodings(const json& offerMediaObject)
		{
			std::set<uint32_t> ssrcs;

			for (auto& line : offerMediaObject["ssrcs"])
			{
				auto ssrc = line["id"].get<uint32_t>();
				ssrcs.insert(ssrc);
			}

			if (ssrcs.empty())
				throw new std::runtime_error("no a=ssrc lines found");

			// Get media and RTX SSRCs.

			std::map<uint32_t, uint32_t> ssrcToRtxSsrc;

			auto it = offerMediaObject.find("ssrcGroups");
			if (it != offerMediaObject.end())
			{
				auto ssrcGroups = (*it);

				// First assume RTX is used.
				for (auto& line : ssrcGroups)
				{
					if (line["semantics"].get<std::string>() != "FID")
						continue;

					auto fidLine = line["ssrcs"].get<std::string>();
					auto v       = mediasoupclient::Utils::split(fidLine, ' ');

					auto ssrc    = std::stoull(v[0]);
					auto rtxSsrc = std::stoull(v[1]);

					if (ssrcs.find(ssrc) != ssrcs.end())
					{
						// Remove both the SSRC and RTX SSRC from the Set so later we know that they
						// are already handled.
						ssrcs.erase(ssrc);
						ssrcs.erase(rtxSsrc);
					}

					// Add to the map.
					ssrcToRtxSsrc[ssrc] = rtxSsrc;
				}
			}

			// If the Set of SSRCs is not empty it means that RTX is not being used, so take
			// media SSRCs from there.
			for (auto& ssrc : ssrcs)
			{
				// Add to the map.
				ssrcToRtxSsrc[ssrc] = 0u;
			}

			// Fill RTP parameters.

			auto encodings = json::array();

			for (auto& kv : ssrcToRtxSsrc)
			{
				json encoding = { { "ssrc", kv.first } };

				if (kv.second != 0u)
					encoding["rtx"] = { { "ssrc", kv.second } };

				encodings.push_back(encoding);
			}

			return encodings;
		}

		void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject)
		{
			MSC_TRACE();

			for (auto& codec : offerRtpParameters["codecs"])
			{
				auto mimeType = codec["mimeType"].get<std::string>();
				std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

				// Avoid parsing codec parameters for unhandled codecs.
				if (mimeType != "audio/opus")
					continue;

				auto rtps = answerMediaObject["rtp"];
				auto it   = find_if(rtps.begin(), rtps.end(), [&codec](const json& r) {
          return r["payload"] == codec["payloadType"];
        });

				if (it == rtps.end())
					continue;

				// Just in case.
				if (answerMediaObject.find("fmtp") == answerMediaObject.end())
					answerMediaObject["fmtp"] = json::array();

				auto fmtps = answerMediaObject["fmtp"];
				it         = find_if(fmtps.begin(), fmtps.end(), [&codec](const json& f) {
          return f["payload"] == codec["payloadType"];
        });

				json parameters;

				if (it == fmtps.end())
				{
					json fmtp = { { "payload", codec["payloadType"] }, { "config", "" } };

					answerMediaObject["fmtp"].push_back(fmtp);
					parameters = sdptransform::parseParams(fmtp["config"]);
				}
				else
				{
					auto parameters = sdptransform::parseParams((*it)["config"]);
				}

				if (mimeType == "audio/opus")
				{
					auto it2 = codec["parameters"].find("sprop-stereo");

					if (it2 != codec["parameters"].end())
					{
						auto spropStereo     = (*it2).get<uint8_t>();
						parameters["stereo"] = spropStereo != 0u ? 1 : 0;
					}
				}

				// Write the codec fmtp.config back.
				std::ostringstream config;
				for (auto& item : parameters.items())
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

				answerMediaObject["fmtp"][0]["config"] = config.str();
			}
		}
	} // namespace Utils
} // namespace Sdp
} // namespace mediasoupclient
