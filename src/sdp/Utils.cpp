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

			std::set<std::string> ssrcs;

			for (auto& line : mSection["ssrcs"])
			{
				if (line["attribute"].get<std::string>() != "msid")
					continue;

				auto ssrc = std::to_string(line["id"].get<uint32_t>());

				ssrcs.insert(ssrc);

				if (firstSsrc.empty())
					firstSsrc = ssrc;
			}

			MSC_ASSERT(!ssrcs.empty(), "no a=ssrc lines found");

			// Get media and RTX SSRCs.

			std::map<std::string, std::string> ssrcToRtxSsrc;

			// First assume RTX is used.
			for (auto& line : mSection["ssrcGroups"])
			{
				if (line["semantics"].get<std::string>() != "FID")
					continue;

				auto fidLine = line["ssrcs"].get<std::string>();
				auto v       = mediasoupclient::Utils::split(fidLine, ' ');

				auto ssrc    = v[0];
				auto rtxSsrc = v[1];

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
				ssrcToRtxSsrc[ssrc] = std::string();
			}

			// Fill RTP parameters.

			rtpParameters["encodings"] = json::array();

			auto simulcast              = ssrcToRtxSsrc.size() > 1;
			json simulcastSpatialLayers = { "low", "medium", "high" };

			for (auto& kv : ssrcToRtxSsrc)
			{
				json encoding = { { "ssrc", kv.first } };

				if (!kv.second.empty())
					encoding["rtx"] = { "ssrc", kv.second };

				if (simulcast)
				{
					encoding["spatialLayer"] = simulcastSpatialLayers[0];
					simulcastSpatialLayers.erase(0);
				}

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

			auto ssrcs = mSection["ssrcs"];

			auto it = std::find_if(ssrcs.begin(), ssrcs.end(), [](const json& line) {
				return line["attribute"].get<std::string>() == "msid";
			});

			if (it == ssrcs.end())
				throw Exception("a=ssrc line with msid information not found");

			auto ssrcMsidLine = *it;

			auto ssrc = ssrcMsidLine["id"].get<std::uint32_t>();

			auto v    = mediasoupclient::Utils::split(ssrcMsidLine["value"].get<std::string>(), ' ');
			auto msid = v[0];

			uint32_t rtxSsrc{ 0 };

			// Get the SSRC for RTX.

			it = mSection.find("ssrcGroups");
			if (it != mSection.end())
			{
				auto ssrcGroups = *it;

				auto it =
				  std::find_if(ssrcGroups.begin(), ssrcGroups.end(), [&ssrc, &rtxSsrc](const json& line) {
					  if (line["semantics"].get<std::string>() != "FID")
						  return false;

					  auto v = mediasoupclient::Utils::split(line["ssrcs"].get<std::string>(), ' ');
					  if (std::stol(v[0]) == ssrc)
					  {
						  rtxSsrc = std::stol(v[1]);

						  return true;
					  }

					  return false;
				  });
			}

			it = std::find_if(ssrcs.begin(), ssrcs.end(), [](const json& line) {
				return line["attribute"].get<std::string>() == "cname";
			});

			if (it == ssrcs.end())
				throw Exception("CNAME line not found");

			auto cname = (*it)["value"].get<std::string>();

			auto ssrc2 = ssrc + 1;
			auto ssrc3 = ssrc + 2;

			mSection["ssrcGroups"] = json::array();
			mSection["ssrcs"]      = json::array();

			std::string ssrcsLine;
			ssrcsLine.append(std::to_string(ssrc)).append(" ");
			ssrcsLine.append(std::to_string(ssrc2)).append(" ");
			if (numStreams == 3)
				ssrcsLine.append(std::to_string(ssrc3));

			std::string msidValue;
			msidValue.append(msid).append(track->id());

			/* clang-format off */
			mSection["ssrcGroups"].push_back(
				{
					{ "semantics", "SIM"     },
					{ "ssrcs",     ssrcsLine },
				});

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

			mSection["ssrcs"].push_back(
				{
					{ "id",        ssrc2   },
					{ "attribute", "cname" },
					{ "value",     cname   }
				});

			mSection["ssrcs"].push_back(
				{
					{ "id",        ssrc2     },
					{ "attribute", "msid"    },
					{ "value",     msidValue }
				});

			if (numStreams == 3)
			{
				mSection["ssrcs"].push_back(
					{
						{ "id",        ssrc3   },
						{ "attribute", "cname" },
						{ "value",     cname   }
					});

				mSection["ssrcs"].push_back(
					{
						{ "id",        ssrc3     },
						{ "attribute", "msid"    },
						{ "value",     msidValue }
					});
			}

			if (rtxSsrc != 0u)
			{
				std::string fid;

				auto rtxSsrc2 = rtxSsrc + 1;
				auto rtxSsrc3 = rtxSsrc + 2;

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

				fid = std::to_string(ssrc2).append(" ");
				fid.append(std::to_string(rtxSsrc2));
				mSection["ssrcGroups"].push_back(
					{
						{ "semantics", "FID" },
						{ "ssrcs",     fid   }
					});

				mSection["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc2 },
						{ "attribute", "cname"  },
						{ "value",     cname    }
					});

				mSection["ssrcs"].push_back(
					{
						{ "id",        rtxSsrc2  },
						{ "attribute", "msid"    },
						{ "value",     msidValue }
					});

				fid = std::to_string(ssrc3).append(" ");
				fid.append(std::to_string(rtxSsrc3));
				if (numStreams == 3)
				{
					mSection["ssrcGroups"].push_back(
						{
							{ "semantics", "FID" },
							{ "ssrcs",     fid   }
						});

					mSection["ssrcs"].push_back(
						{
							{ "id",        rtxSsrc3 },
							{ "attribute", "cname"  },
							{ "value",     cname    }
						});

					mSection["ssrcs"].push_back(
						{
							{ "id",       rtxSsrc3   },
							{ "attribute", "msid"    },
							{ "value",    msidValue  }
						});
				}
			}
			/* clang-format on */
		};
	} // namespace Utils
} // namespace Sdp
} // namespace mediasoupclient
