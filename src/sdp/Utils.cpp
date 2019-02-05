#define MSC_CLASS "Sdp::Utils"
// #define MSC_LOG_DEV

#include "sdp/Utils.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
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

		size_t findMediaSection(const json& sdpObj, const std::string& mid)
		{
			json mSection;

			auto media = sdpObj["media"];

			auto it = std::find_if(media.begin(), media.end(), [&mid](const json& m) {
				return mid == m["mid"].get<std::string>();
			});

			MSC_ASSERT(it != media.end(), "a=mid line with msid information not found");

			return it - media.begin();
		};

		size_t findMediaSection(const json& sdpObj, const webrtc::MediaStreamTrackInterface* track)
		{
			auto media = sdpObj["media"];

			// Find a media section with 'msid' value pointing to the given track.
			auto it = std::find_if(media.begin(), media.end(), [&track](const json& m) {
				auto it2 = m.find("msid");
				if (it2 == m.end())
					return false;

				auto line = (*it2).get<std::string>();
				auto v    = mediasoupclient::Utils::split(line, ' ');

				auto msid = v[1];

				return msid == track->id();
			});

			MSC_ASSERT(it != media.end(), "a=msid line with msid information not found");

			return it - media.begin();
		};

		void fillRtpParametersForTrack(json& rtpParameters, const json& sdpObj, const std::string& mid)
		{
			MSC_TRACE();

			auto index    = findMediaSection(sdpObj, mid);
			auto mSection = sdpObj["media"][index];

			rtpParameters["mid"] = mid;

			/* clang-format off */
			rtpParameters["rtcp"] =
			{
				{ "cname",       nullptr },
				{ "reducedSize", true    },
				{ "mux",         true    }
			};
			/* clang-format on */

			// Get the SSRC and CNAME.

			auto mSsrcs = mSection["ssrcs"];

			auto it = std::find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
				return line["attribute"].get<std::string>() == "cname";
			});

			MSC_ASSERT(it != mSsrcs.end(), "CNAME value not found");

			rtpParameters["rtcp"]["cname"] = (*it)["value"];

			// Simulcast based on PlanB.

			// First media SSRC (or the only one).
			std::string firstSsrc;

			// Get all the SSRCs.

			std::set<uint32_t> ssrcs;

			for (auto& line : mSection["ssrcs"])
			{
				if (line["attribute"].get<std::string>() != "msid")
					continue;

				auto ssrc = line["id"].get<uint32_t>();

				ssrcs.insert(ssrc);

				if (firstSsrc.empty())
					firstSsrc = std::to_string(ssrc);
			}

			MSC_ASSERT(!ssrcs.empty(), "no a=ssrc lines found");

			// Get media and RTX SSRCs.

			std::map<uint32_t, uint32_t> ssrcToRtxSsrc;

			// First assume RTX is used.
			for (auto& line : mSection["ssrcGroups"])
			{
				if (line["semantics"].get<std::string>() != "FID")
					continue;

				auto fidLine = line["ssrcs"].get<std::string>();
				auto v       = mediasoupclient::Utils::split(fidLine, ' ');

				auto ssrc    = std::stol(v[0]);
				auto rtxSsrc = std::stol(v[1]);

				// Remove both the SSRC and RTX SSRC from the Set so later we know that they
				// are already handled.
				ssrcs.erase(ssrc);
				ssrcs.erase(rtxSsrc);

				// Add to the map.
				ssrcToRtxSsrc[ssrc] = rtxSsrc;
			}

			// If the Set of SSRCs is not empty it means that RTX is not being used, so take
			// media SSRCs from there.
			for (auto& ssrc : ssrcs)
			{
				// Add to the map.
				ssrcToRtxSsrc[ssrc] = 0u;
			}

			// Fill RTP parameters.

			rtpParameters["encodings"] = json::array();

			for (auto& kv : ssrcToRtxSsrc)
			{
				json encoding = { { "ssrc", kv.first } };

				if (kv.second != 0u)
					encoding["rtx"] = { { "ssrc", kv.second } };

				rtpParameters["encodings"].push_back(encoding);
			}
		};

		void addLegacySimulcast(
		  json& sdpObj, const webrtc::MediaStreamTrackInterface* track, uint8_t numStreams)
		{
			MSC_TRACE();

			if (numStreams <= 1)
				return;

			auto index     = findMediaSection(sdpObj, track);
			auto& mSection = sdpObj["media"][index];

			// Get the SSRC.

			auto mSsrcs = mSection["ssrcs"];

			auto it = std::find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
				return line["attribute"].get<std::string>() == "msid";
			});

			if (it == mSsrcs.end())
				throw Exception("a=ssrc line with msid information not found");

			auto ssrcMsidLine = *it;

			auto v    = mediasoupclient::Utils::split(ssrcMsidLine["value"].get<std::string>(), ' ');
			auto msid = v[0];

			auto firstSsrc = ssrcMsidLine["id"].get<std::uint32_t>();
			uint32_t firstRtxSsrc{ 0 };

			// Get the SSRC for RTX.

			it = mSection.find("ssrcGroups");
			if (it != mSection.end())
			{
				auto ssrcGroups = *it;

				std::find_if(
				  ssrcGroups.begin(), ssrcGroups.end(), [&firstSsrc, &firstRtxSsrc](const json& line) {
					  if (line["semantics"].get<std::string>() != "FID")
						  return false;

					  auto v = mediasoupclient::Utils::split(line["ssrcs"].get<std::string>(), ' ');
					  if (std::stol(v[0]) == firstSsrc)
					  {
						  firstRtxSsrc = std::stol(v[1]);

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

			mSection["ssrcGroups"] = json::array();
			mSection["ssrcs"]      = json::array();

			std::vector<uint32_t> ussrcs = ssrcs;
			auto ssrcsLine               = mediasoupclient::Utils::join(ussrcs, ' ');

			std::string msidValue(msid);
			msidValue.append(" ").append(track->id());

			/* clang-format off */
			mSection["ssrcGroups"].push_back(
				{
					{ "semantics", "SIM"     },
					{ "ssrcs",     ssrcsLine },
				});

			for (auto &i : ssrcs)
			{
				auto ssrc = i.get<uint32_t>();

				mSection["ssrcs"].push_back(
					{
						{ "id",        ssrc    },
						{ "attribute", "cname" },
						{ "value",     cname   }
					});

				mSection["ssrcs"].push_back(
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

				mSection["ssrcGroups"].push_back(
					{
						{ "semantics", "FID" },
						{ "ssrcs",     fid   }
					});

				mSection["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc },
						{ "attribute", "cname" },
						{ "value",     cname   }
					});

				mSection["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc   },
						{ "attribute", "msid"    },
						{ "value",     msidValue }
					});
			}
			/* clang-format on */
		};
	} // namespace Utils
} // namespace Sdp
} // namespace mediasoupclient
