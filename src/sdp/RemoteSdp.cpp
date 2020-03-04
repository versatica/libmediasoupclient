#define MSC_CLASS "Sdp::RemoteSdp"

#include "sdp/RemoteSdp.hpp"
#include "Logger.hpp"
#include "sdptransform.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	/* Sdp::RemoteSdp methods */

	Sdp::RemoteSdp::RemoteSdp(
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters)
	  : iceParameters(iceParameters), iceCandidates(iceCandidates), dtlsParameters(dtlsParameters),
	    sctpParameters(sctpParameters)
	{
		MSC_TRACE();

		// clang-format off
		this->sdpObject =
		{
			{ "version", 0 },
			{ "origin",
				{
					{ "address",        "0.0.0.0"                        },
					{ "ipVer",          4                                },
					{ "netType",        "IN"                             },
					{ "sessionId",      10000                            },
					{ "sessionVersion", 0                                },
					{ "username",       "libmediasoupclient"             }
				}
			},
			{ "name", "-" },
		  { "timing",
				{
					{ "start", 0 },
					{ "stop",  0 }
				}
			},
			{ "media", json::array() }
		};
		// clang-format on

		// If ICE parameters are given, add ICE-Lite indicator.
		if (this->iceParameters.find("iceLite") != this->iceParameters.end())
			this->sdpObject["icelite"] = "ice-lite";

		// clang-format off
		this->sdpObject["msidSemantic"] =
		{
			{ "semantic", "WMS" },
			{ "token",    "*"   }
		};
		// clang-format on

		// NOTE: We take the latest fingerprint.
		auto numFingerprints = this->dtlsParameters["fingerprints"].size();

		this->sdpObject["fingerprint"] = {
			{ "type", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["algorithm"] },
			{ "hash", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["value"] }
		};

		// clang-format off
		this->sdpObject["groups"] =
		{
			{
				{ "type", "BUNDLE" },
				{ "mids", ""       }
			}
		};
		// clang-format on
	}

	void Sdp::RemoteSdp::UpdateIceParameters(const json& iceParameters)
	{
		MSC_TRACE();

		this->iceParameters = iceParameters;

		if (iceParameters.find("iceLite") != iceParameters.end())
			sdpObject["icelite"] = "ice-lite";

		for (auto& kv : this->mediaSections)
		{
			auto* mediaSection = kv.second;

			mediaSection->SetIceParameters(iceParameters);
		}
	}

	void Sdp::RemoteSdp::UpdateDtlsRole(const std::string& role)
	{
		MSC_TRACE();

		this->dtlsParameters["role"] = role;

		if (iceParameters.find("iceLite") != iceParameters.end())
			sdpObject["icelite"] = "ice-lite";

		for (auto& kv : this->mediaSections)
		{
			auto* mediaSection = kv.second;

			mediaSection->SetDtlsRole(role);
		}
	}

	Sdp::RemoteSdp::MediaSectionIdx Sdp::RemoteSdp::GetNextMediaSectionIdx()
	{
		MSC_TRACE();

		size_t idx = 0;

		// If a closed media section is found, return its index.
		for (const auto& kv : this->mediaSections)
		{
			auto* mediaSection = kv.second;
			if (mediaSection->IsClosed())
				return { idx, mediaSection->GetMid() };

			idx++;
		}

		// If no closed media section is found, return next one.
		return { this->mediaSections.size() };
	}

	void Sdp::RemoteSdp::Send(
	  json& offerMediaObject, std::string reuseMid, json& offerRtpParameters, json& answerRtpParameters, const json* codecOptions)
	{
		MSC_TRACE();

		auto* mediaSection = new AnswerMediaSection(
		  this->iceParameters,
		  this->iceCandidates,
		  this->dtlsParameters,
		  this->sctpParameters,
		  offerMediaObject,
		  offerRtpParameters,
		  answerRtpParameters,
		  codecOptions);

		// Closed media section replacement.
		if (!reuseMid.empty())
		{
			this->ReplaceMediaSection(mediaSection, reuseMid);
		}
		else
		{
			this->AddMediaSection(mediaSection);
		}
	}

	void Sdp::RemoteSdp::Receive(
	  const std::string& mid,
	  const std::string& kind,
	  const json& offerRtpParameters,
	  const std::string& streamId,
	  const std::string& trackId)
	{
		MSC_TRACE();

		auto* mediaSection = new OfferMediaSection(
		  this->iceParameters,
		  this->iceCandidates,
		  this->dtlsParameters,
		  nullptr, // sctpParameters must be null here.
		  mid,
		  kind,
		  offerRtpParameters,
		  streamId,
		  trackId);

		this->AddMediaSection(mediaSection);
	}

	void Sdp::RemoteSdp::DisableMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		// TODO: Should check that mediaSections.find(mid) exists.
		// TODO: Should also really serialize the SDP with a=inactive!

		auto* mediaSection = this->mediaSections[mid];

		mediaSection->Disable();

		// Update SDP media sections.
		this->UpdateMediaSectionObject();
	}

	void Sdp::RemoteSdp::CloseMediaSection(std::string mid)
	{
		MSC_TRACE();

		MSC_ERROR("mid: %s", mid.c_str());

		auto* mediaSection = this->mediaSections[mid];

		// NOTE: Closing the first m section is a pain since it invalidates the
		// bundled transport, so let's avoid it.
		if (mid == this->firstMid)
		{
			MSC_DEBUG(
				"cannot close first media section, disabling it instead [mid:%s]",
				mid.c_str());

			this->DisableMediaSection(mid);

			return;
		}

		mediaSection->Close();

		// Regenerate BUNDLE mids.
		this->RegenerateBundleMids();

		// Regenerate media sections.
		this->UpdateMediaSectionObject();
	}

	std::string Sdp::RemoteSdp::GetSdp()
	{
		MSC_TRACE();

		// Increase SDP version.
		auto version = this->sdpObject["origin"]["sessionVersion"].get<uint32_t>();

		this->sdpObject["origin"]["sessionVersion"] = ++version;

		return sdptransform::write(this->sdpObject);
	}

	void Sdp::RemoteSdp::AddMediaSection(MediaSection* newMediaSection)
	{
		MSC_TRACE();

		if (this->firstMid.empty())
			this->firstMid = newMediaSection->GetMid();

		// Store it in the map.
		this->mediaSections[newMediaSection->GetMid()] = newMediaSection;

		// Update SDP object.
		this->sdpObject["media"].push_back(newMediaSection->GetObject());

		this->RegenerateBundleMids();
	}

	void Sdp::RemoteSdp::ReplaceMediaSection(MediaSection* newMediaSection, std::string reuseMid)
	{
		MSC_TRACE();

		// Store it in the map.
		if (!reuseMid.empty())
		{
			std::map<std::string, MediaSection*> newMediaSections;

			for (const auto& kv : this->mediaSections)
			{
				auto* mediaSection = kv.second;

				if (mediaSection->GetMid() == reuseMid)
					newMediaSections[newMediaSection->GetMid()] = newMediaSection;
				else
					newMediaSections[mediaSection->GetMid()] = mediaSection;
			}

			// Regenerate media sections.
			this->mediaSections = newMediaSections;

			// Regenerate BUNDLE mids.
			this->RegenerateBundleMids();
		}
		else
		{
			this->mediaSections[newMediaSection->GetMid()] = newMediaSection;
		}

		// Update SDP media sections.
		this->UpdateMediaSectionObject();
	}

	void Sdp::RemoteSdp::RegenerateBundleMids()
	{
		MSC_TRACE();

		std::string mids;

		for (const auto& kv : this->mediaSections)
		{
			const auto* mediaSection = kv.second;

			MSC_ERROR("mediaSection->GetMid(): %s", mediaSection->GetMid().c_str());

			if (!mediaSection->IsClosed())
			{
				if (mids.empty())
					mids = mediaSection->GetMid();
				else
					mids.append(" ").append(mediaSection->GetMid());
			}
		}

		this->sdpObject["groups"][0]["mids"] = mids;
		MSC_ERROR("groups 0 mids: %s", mids.c_str());
	}

	void Sdp::RemoteSdp::UpdateMediaSectionObject()
	{
		MSC_TRACE();

		this->sdpObject["media"] = json::array();

		for (const auto& kv : this->mediaSections)
		{
			const auto* mediaSection = kv.second;

			this->sdpObject["media"].push_back(mediaSection->GetObject());
		}
	}
} // namespace mediasoupclient
