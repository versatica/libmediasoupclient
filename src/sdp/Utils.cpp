#define MSC_CLASS "Sdp::Utils"

#include "sdp/Utils.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include "Utils.hpp"
#include <algorithm> // ::transform
#include <cctype>    // ::tolower
#include <list>
#include <map>
#include <vector>

namespace mediasoupclient
{
	namespace Sdp
	{
		namespace Utils
		{
			json extractRtpCapabilities(const json& sdpObject)
			{
				MSC_TRACE();

				// Map of RtpCodecParameters indexed by payload type.
				std::map<uint8_t, json> codecsMap;

				// Array of RtpHeaderExtensions.
				auto headerExtensions = json::array();

				// Whether a m=audio/video section has been already found.
				bool gotAudio = false;
				bool gotVideo = false;

				for (const auto& m : sdpObject["media"])
				{
					auto kind = m["type"].get<std::string>();

					if (kind == "audio")
					{
						if (gotAudio)
						{
							continue;
						}

						gotAudio = true;
					}
					else if (kind == "video")
					{
						if (gotVideo)
						{
							continue;
						}

						gotVideo = true;
					}
					else
					{
						continue;
					}

					// Get codecs.
					const auto jsonRtpIt = m.find("rtp");

					if (jsonRtpIt != m.end() && jsonRtpIt->is_array())
					{
						for (const auto& rtp : *jsonRtpIt)
						{
							std::string mimeType(kind);
							mimeType.append("/").append(rtp["codec"].get<std::string>());

							// clang-format off
							json codec =
							{
								{ "kind",                 kind           },
								{ "mimeType",             mimeType       },
								{ "preferredPayloadType", rtp["payload"] },
								{ "clockRate",            rtp["rate"]    },
								{ "parameters",           json::object() },
								{ "rtcpFeedback",         json::array()  }
							};
							// clang-format on

							if (kind == "audio")
							{
								const auto jsonEncodingIt = rtp.find("encoding");

								if (jsonEncodingIt != rtp.end() && jsonEncodingIt->is_string())
								{
									codec["channels"] = std::stoi(jsonEncodingIt->get<std::string>());
								}
								else
								{
									codec["channels"] = 1;
								}
							}

							codecsMap[codec["preferredPayloadType"].get<uint8_t>()] = codec;
						}
					}

					// Get codec parameters.
					const auto jsonFmtpIt = m.find("fmtp");

					if (jsonFmtpIt != m.end() && jsonFmtpIt->is_array())
					{
						for (const auto& fmtp : *jsonFmtpIt)
						{
							const auto jsonPayloadIt = codecsMap.find(fmtp["payload"]);

							auto parameters = sdptransform::parseParams(fmtp["config"]);

							if (jsonPayloadIt == codecsMap.end())
							{
								continue;
							}

							// If preset, convert 'profile-id' parameter (VP8 and VP9) into
							// integer since we define it that way in mediasoup RtpCodecParameters
							// and RtpCodecCapability.
							const auto profileIdIt = parameters.find("profile-id");

							if (profileIdIt != parameters.end() && profileIdIt->is_string())
							{
								parameters["profile-id"] = std::stoi(profileIdIt->get<std::string>());
							}

							auto& codec = jsonPayloadIt->second;

							codec["parameters"] = parameters;
						}
					}

					// Get RTCP feedback for each codec.
					const auto jsonRtcpFbIt = m.find("rtcpFb");

					if (jsonRtcpFbIt != m.end() && jsonRtcpFbIt->is_array())
					{
						for (const auto& fb : *jsonRtcpFbIt)
						{
							const auto jsonCodecIt = codecsMap.find(std::stoi(fb["payload"].get<std::string>()));

							if (jsonCodecIt == codecsMap.end())
							{
								continue;
							}

							auto& codec = jsonCodecIt->second;

							// clang-format off
							json feedback =
							{
								{"type", fb["type"]}
							};
							// clang-format on

							const auto jsonSubtypeIt = fb.find("subtype");

							if (jsonSubtypeIt != fb.end())
							{
								feedback["parameter"] = *jsonSubtypeIt;
							}

							codec["rtcpFeedback"].push_back(feedback);
						}
					}

					// Get RTP header extensions.
					const auto jsonExtIt = m.find("ext");

					if (jsonExtIt != m.end() && jsonExtIt->is_array())
					{
						for (const auto& ext : *jsonExtIt)
						{
							// clang-format off
							json const headerExtension =
							{
									{ "kind",        kind },
									{ "uri",         ext["uri"] },
									{ "preferredId", ext["value"] }
							};
							// clang-format on

							headerExtensions.push_back(headerExtension);
						}
					}
				}

				// clang-format off
				json rtpCapabilities =
				{
					{ "headerExtensions", headerExtensions },
					{ "codecs",           json::array() },
					{ "fecMechanisms",    json::array() } // TODO
				};
				// clang-format on

				for (const auto& kv : codecsMap)
				{
					const auto& codec = kv.second;

					rtpCapabilities["codecs"].push_back(codec);
				}

				return rtpCapabilities;
			}

			json extractDtlsParameters(const json& sdpObject)
			{
				MSC_TRACE();

				json m;
				json fingerprint;
				std::string role;

				for (const auto& media : sdpObject["media"])
				{
					if (media.contains("iceUfrag") && media["port"] != 0)
					{
						m = media;
						break;
					}
				}

				if (m.contains("fingerprint"))
				{
					fingerprint = m["fingerprint"];
				}
				else if (sdpObject.contains("fingerprint"))
				{
					fingerprint = sdpObject["fingerprint"];
				}

				if (m.contains("setup"))
				{
					const std::string setup = m["setup"];

					if (setup == "active")
					{
						role = "client";
					}
					else if (setup == "passive")
					{
						role = "server";
					}
					else if (setup == "actpass")
					{
						role = "auto";
					}
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
				{
					return;
				}

				// Get the SSRC.
				auto mSsrcs = offerMediaObject["ssrcs"];

				auto jsonSsrcIt = std::ranges::find_if(
				  mSsrcs,

				  [](const json& line) -> bool
				  {
					  const auto jsonAttributeIt = line.find("attribute");

					  if (jsonAttributeIt == line.end() || !jsonAttributeIt->is_string())
					  {
						  return false;
					  }

					  return jsonAttributeIt->get<std::string>() == "msid";
				  });

				if (jsonSsrcIt == mSsrcs.end())
				{
					MSC_THROW_ERROR("a=ssrc line with msid information not found");
				}

				auto& ssrcMsidLine = *jsonSsrcIt;
				auto v = mediasoupclient::Utils::Split(ssrcMsidLine["value"].get<std::string>(), ' ');
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
				const auto& streamId = v[0];
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
				const auto& trackId  = v[1];
				const auto firstSsrc = ssrcMsidLine["id"].get<std::uint32_t>();
				uint32_t firstRtxSsrc{ 0 };

				// Get the SSRC for RTX.

				const auto jsonSsrcGroupsIt = offerMediaObject.find("ssrcGroups");

				if (jsonSsrcGroupsIt != offerMediaObject.end())
				{
					const auto& ssrcGroups = *jsonSsrcGroupsIt;

					for (const auto& line : ssrcGroups)
					{
						const auto jsonSemanticsIt = line.find("semantics");

						if (jsonSemanticsIt == line.end() || !jsonSemanticsIt->is_string())
						{
							continue;
						}

						const auto jsonSsrcsIt = line.find("ssrcs");

						if (jsonSsrcsIt == line.end() || !jsonSsrcsIt->is_string())
						{
							continue;
						}

						auto v = mediasoupclient::Utils::Split(jsonSsrcsIt->get<std::string>(), ' ');

						// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
						if (std::stoull(v[0]) == firstSsrc)
						{
							// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
							firstRtxSsrc = std::stoull(v[1]);

							break;
						}
					};
				}

				jsonSsrcIt = std::ranges::find_if(
				  mSsrcs,

				  [](const json& line) -> bool
				  {
					  const auto jsonAttributeIt = line.find("attribute");
					  if (jsonAttributeIt == line.end() || !jsonAttributeIt->is_string())
					  {
						  return false;
					  }

					  const auto jsonIdIt = line.find("id");
					  if (jsonIdIt == line.end() || !jsonIdIt->is_number())
					  {
						  return false;
					  }

					  return (jsonAttributeIt->get<std::string>() == "cname");
				  });

				if (jsonSsrcIt == mSsrcs.end())
				{
					MSC_THROW_ERROR("CNAME line not found");
				}

				auto cname    = (*jsonSsrcIt)["value"].get<std::string>();
				auto ssrcs    = json::array();
				auto rtxSsrcs = json::array();

				for (uint8_t i = 0; i < numStreams; ++i)
				{
					ssrcs.push_back(firstSsrc + i);

					if (firstRtxSsrc != 0)
					{
						rtxSsrcs.push_back(firstRtxSsrc + i);
					}
				}

				offerMediaObject["ssrcGroups"] = json::array();
				offerMediaObject["ssrcs"]      = json::array();

				const std::vector<uint32_t> ussrcs = ssrcs;
				auto ssrcsLine                     = mediasoupclient::Utils::Join(ussrcs, ' ');

				std::string msidValue(streamId);
				msidValue.append(" ").append(trackId);

				// clang-format off
				offerMediaObject["ssrcGroups"].push_back(
					{
						{ "semantics", "SIM"     },
						{ "ssrcs",     ssrcsLine },
					});
				// clang-format on

				for (const auto& i : ssrcs)
				{
					auto ssrc = i.get<uint32_t>();

					// clang-format off
					offerMediaObject["ssrcs"].push_back(
						{
							{ "id",        ssrc    },
							{ "attribute", "cname" },
							{ "value",     cname   }
						});
					// clang-format on

					// clang-format off
					offerMediaObject["ssrcs"].push_back(
						{
							{ "id",        ssrc      },
							{ "attribute", "msid"    },
							{ "value",     msidValue }
						});
					// clang-format on
				}

				for (size_t i = 0; i < rtxSsrcs.size(); ++i)
				{
					const auto ssrc = ssrcs[i].get<uint32_t>();
					auto rtxSsrc    = rtxSsrcs[i].get<uint32_t>();

					std::string fid;
					fid = std::to_string(ssrc).append(" ");
					fid.append(std::to_string(rtxSsrc));

					// clang-format off
					offerMediaObject["ssrcGroups"].push_back(
						{
							{ "semantics", "FID" },
							{ "ssrcs",     fid   }
						});
					// clang-format on

					// clang-format off
					offerMediaObject["ssrcs"].push_back(
						{
							{ "id",        rtxSsrc },
							{ "attribute", "cname" },
							{ "value",     cname   }
						});
					// clang-format on

					// clang-format off
					offerMediaObject["ssrcs"].push_back(
						{
							{ "id",        rtxSsrc   },
							{ "attribute", "msid"    },
							{ "value",     msidValue }
						});
					// clang-format on
				}
			};

			std::string getCname(const json& offerMediaObject)
			{
				MSC_TRACE();

				const auto jsonMssrcsIt = offerMediaObject.find("ssrcs");

				if (jsonMssrcsIt == offerMediaObject.end())
				{
					return "";
				}

				const json& mSsrcs = *jsonMssrcsIt;

				const auto jsonSsrcIt = std::ranges::find_if(
				  mSsrcs,

				  [](const json& line) -> bool
				  {
					  const auto jsonAttributeIt = line.find("attribute");

					  return (jsonAttributeIt != line.end() && jsonAttributeIt->is_string());
				  });

				if (jsonSsrcIt == mSsrcs.end())
				{
					return "";
				}

				const auto& ssrcCnameLine = *jsonSsrcIt;

				return ssrcCnameLine["value"].get<std::string>();
			}

			json getRtpEncodings(const json& offerMediaObject)
			{
				MSC_TRACE();

				std::list<uint32_t> ssrcs;

				for (const auto& line : offerMediaObject["ssrcs"])
				{
					const auto ssrc = line["id"].get<uint32_t>();
					ssrcs.push_back(ssrc);
				}

				if (ssrcs.empty())
				{
					MSC_THROW_ERROR("no a=ssrc lines found");
				}

				ssrcs.unique();

				// Get media and RTX SSRCs.

				std::map<uint32_t, uint32_t> ssrcToRtxSsrc;

				const auto jsonSsrcGroupsIt = offerMediaObject.find("ssrcGroups");
				if (jsonSsrcGroupsIt != offerMediaObject.end())
				{
					const auto& ssrcGroups = *jsonSsrcGroupsIt;

					// First assume RTX is used.
					for (const auto& line : ssrcGroups)
					{
						if (line["semantics"].get<std::string>() != "FID")
						{
							continue;
						}

						const auto fidLine = line["ssrcs"].get<std::string>();
						auto v             = mediasoupclient::Utils::Split(fidLine, ' ');
						// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
						auto ssrc = std::stoull(v[0]);
						// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
						const auto rtxSsrc = std::stoull(v[1]);

						// Remove the RTX SSRC from the List so later we know that they
						// are already handled.
						ssrcs.remove(rtxSsrc);

						// Add to the map.
						ssrcToRtxSsrc[ssrc] = rtxSsrc;
					}
				}

				// Fill RTP parameters.

				auto encodings = json::array();

				for (auto& ssrc : ssrcs)
				{
					json encoding = {
						{ "ssrc", ssrc }
					};

					const auto it = ssrcToRtxSsrc.find(ssrc);

					if (it != ssrcToRtxSsrc.end())
					{
						encoding["rtx"] = {
							{ "ssrc", it->second }
						};
					}

					encodings.push_back(encoding);
				}

				return encodings;
			}

			void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject)
			{
				MSC_TRACE();

				for (const auto& codec : offerRtpParameters["codecs"])
				{
					auto mimeType = codec["mimeType"].get<std::string>();

					std::ranges::transform(mimeType, mimeType.begin(), ::tolower);

					// Avoid parsing codec parameters for unhandled codecs.
					if (mimeType != "audio/opus")
					{
						continue;
					}

					auto& rtps = answerMediaObject["rtp"];

					const auto jsonRtpIt = std::ranges::find_if(
					  rtps,

					  [&codec](const json& r) -> bool
					  {
						  return r["payload"] == codec["payloadType"];
					  });

					if (jsonRtpIt == rtps.end())
					{
						continue;
					}

					// Just in case.
					if (!answerMediaObject.contains("fmtp"))
					{
						answerMediaObject["fmtp"] = json::array();
					}

					auto& fmtps     = answerMediaObject["fmtp"];
					auto jsonFmtpIt = std::ranges::find_if(
					  fmtps,

					  [&codec](const json& f) -> bool
					  {
						  return f["payload"] == codec["payloadType"];
					  });

					if (jsonFmtpIt == fmtps.end())
					{
						const json fmtp = {
							{ "payload", codec["payloadType"] },
              { "config",  ""                   }
						};

						answerMediaObject["fmtp"].push_back(fmtp);
						jsonFmtpIt = answerMediaObject["fmtp"].end() - 1;
					}

					json& fmtp      = *jsonFmtpIt;
					json parameters = sdptransform::parseParams(fmtp["config"]);

					if (mimeType == "audio/opus")
					{
						const auto jsonSpropStereoIt = codec["parameters"].find("sprop-stereo");

						if (jsonSpropStereoIt != codec["parameters"].end() && jsonSpropStereoIt->is_number())
						{
							const auto spropStereo = jsonSpropStereoIt->get<int>();

							parameters["stereo"] = spropStereo ? 1 : 0;
						}
					}

					// Write the codec fmtp.config back.
					std::ostringstream config;

					for (const auto& item : parameters.items())
					{
						if (!config.str().empty())
						{
							config << ";";
						}

						config << item.key();
						config << "=";
						if (item.value().is_string())
						{
							config << item.value().get<std::string>();
						}
						else if (item.value().is_number_float())
						{
							config << item.value().get<float>();
						}
						else if (item.value().is_number())
						{
							config << item.value().get<int>();
						}
					}

					fmtp["config"] = config.str();
				}
			}
		} // namespace Utils
	} // namespace Sdp
} // namespace mediasoupclient
