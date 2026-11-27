#define MSC_CLASS "Sdp::RemoteSdp"

#include "sdp/RemoteSdp.hpp"
#include "Logger.hpp"
#include "sdptransform.hpp"
#include <algorithm> // find_if.

using nlohmann::json;

namespace mediasoupclient
{
	/* Sdp::RemoteSdp methods */

	Sdp::RemoteSdp::RemoteSdp(
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters)
	  : iceParameters(iceParameters),
	    iceCandidates(iceCandidates),
	    dtlsParameters(dtlsParameters),
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
		if (this->iceParameters.contains("iceLite"))
		{
			this->sdpObject["icelite"] = "ice-lite";
		}

		// clang-format off
		this->sdpObject["msidSemantic"] =
		{
			{ "semantic", "WMS" },
			{ "token",    "*"   }
		};
		// clang-format on

		// NOTE: We take the latest fingerprint.
		const auto numFingerprints = this->dtlsParameters["fingerprints"].size();

		this->sdpObject["fingerprint"] = {
			{ "type", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["algorithm"] },
			{ "hash", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["value"]     }
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

	Sdp::RemoteSdp::~RemoteSdp()
	{
		MSC_TRACE();
	}

	void Sdp::RemoteSdp::UpdateIceParameters(const json& iceParameters)
	{
		MSC_TRACE();

		this->iceParameters = iceParameters;

		if (iceParameters.contains("iceLite"))
		{
			sdpObject["icelite"] = "ice-lite";
		}

		for (auto idx{ 0U }; idx < this->mediaSections.size(); ++idx)
		{
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			const auto& mediaSection = this->mediaSections[idx];

			mediaSection->SetIceParameters(iceParameters);

			// Update SDP media section.
			this->sdpObject["media"][idx] = mediaSection->GetObject();
		}
	}

	void Sdp::RemoteSdp::UpdateDtlsRole(const std::string& role)
	{
		MSC_TRACE();

		this->dtlsParameters["role"] = role;

		if (iceParameters.contains("iceLite"))
		{
			sdpObject["icelite"] = "ice-lite";
		}

		for (auto idx{ 0U }; idx < this->mediaSections.size(); ++idx)
		{
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			const auto& mediaSection = this->mediaSections[idx];

			mediaSection->SetDtlsRole(role);

			// Update SDP media section.
			this->sdpObject["media"][idx] = mediaSection->GetObject();
		}
	}

	Sdp::RemoteSdp::MediaSectionIdx Sdp::RemoteSdp::GetNextMediaSectionIdx()
	{
		MSC_TRACE();

		// If a closed media section is found, return its index.
		for (auto idx{ 0U }; idx < this->mediaSections.size(); ++idx)
		{
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			const auto& mediaSection = this->mediaSections[idx];

			if (mediaSection->IsClosed())
			{
				return { .idx = idx, .reuseMid = mediaSection->GetMid() };
			}
		}

		// If no closed media section is found, return next one.
		return { .idx = this->mediaSections.size() };
	}

	void Sdp::RemoteSdp::Send(
	  json& offerMediaObject,
	  const std::string& reuseMid,
	  json& offerRtpParameters,
	  json& answerRtpParameters,
	  const json* codecOptions)
	{
		MSC_TRACE();

		auto mediaSection = std::make_unique<AnswerMediaSection>(
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
			ReplaceMediaSection(std::move(mediaSection), reuseMid);
		}
		else
		{
			AddMediaSection(std::move(mediaSection));
		}
	}

	void Sdp::RemoteSdp::SendSctpAssociation(json& offerMediaObject)
	{
		MSC_TRACE();

		nlohmann::json emptyJson;
		auto mediaSection = std::make_unique<AnswerMediaSection>(
		  this->iceParameters,
		  this->iceCandidates,
		  this->dtlsParameters,
		  this->sctpParameters,
		  offerMediaObject,
		  emptyJson,
		  emptyJson,
		  nullptr);

		AddMediaSection(std::move(mediaSection));
	}

	void Sdp::RemoteSdp::RecvSctpAssociation()
	{
		MSC_TRACE();

		const nlohmann::json emptyJson;
		auto mediaSection = std::make_unique<OfferMediaSection>(
		  this->iceParameters,
		  this->iceCandidates,
		  this->dtlsParameters,
		  this->sctpParameters,
		  "datachannel", // mid
		  "application", // kind
		  emptyJson,     // offerRtpParameters
		  "",            // streamId
		  ""             // trackId
		);
		AddMediaSection(std::move(mediaSection));
	}

	void Sdp::RemoteSdp::Receive(
	  const std::string& mid,
	  const std::string& kind,
	  const json& offerRtpParameters,
	  const std::string& streamId,
	  const std::string& trackId)
	{
		MSC_TRACE();

		auto mediaSection = std::make_unique<OfferMediaSection>(
		  this->iceParameters,
		  this->iceCandidates,
		  this->dtlsParameters,
		  nullptr, // sctpParameters must be null here.
		  mid,
		  kind,
		  offerRtpParameters,
		  streamId,
		  trackId);

		// Let's try to recycle a closed media section (if any).
		// NOTE: We can recycle a closed m=audio section with a new m=video.
		const auto mediaSectionIt = std::ranges::find_if(
		  this->mediaSections,

		  [](const std::unique_ptr<MediaSection>& mediaSection) -> bool
		  {
			  return mediaSection->IsClosed();
		  });

		if (mediaSectionIt != this->mediaSections.end())
		{
			const auto reuseMid = (*mediaSectionIt)->GetMid();

			ReplaceMediaSection(std::move(mediaSection), reuseMid);
		}
		else
		{
			AddMediaSection(std::move(mediaSection));
		}
	}

	void Sdp::RemoteSdp::PauseMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		const auto idx = this->midToIndex[mid];
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
		const auto& mediaSection = this->mediaSections[idx];

		mediaSection->Pause();
	}

	void Sdp::RemoteSdp::ResumeSendingMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		const auto idx = this->midToIndex[mid];
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
		const auto& mediaSection = this->mediaSections[idx];

		mediaSection->Resume();
	}

	void Sdp::RemoteSdp::ResumeReceivingMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		const auto idx = this->midToIndex[mid];
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
		const auto& mediaSection = this->mediaSections[idx];

		mediaSection->Resume();
	}

	void Sdp::RemoteSdp::DisableMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		const auto idx = this->midToIndex[mid];
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
		const auto& mediaSection = this->mediaSections[idx];

		mediaSection->Disable();
	}

	void Sdp::RemoteSdp::CloseMediaSection(const std::string& mid)
	{
		MSC_TRACE();

		const auto idx = this->midToIndex[mid];
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
		const auto& mediaSection = this->mediaSections[idx];

		// NOTE: Closing the first m section is a pain since it invalidates the
		// bundled transport, so let's avoid it.
		if (mid == this->firstMid)
		{
			mediaSection->Disable();
		}
		else
		{
			mediaSection->Close();
		}

		// Update SDP media section.
		this->sdpObject["media"][idx] = mediaSection->GetObject();

		// Regenerate BUNDLE mids.
		RegenerateBundleMids();
	}

	std::string Sdp::RemoteSdp::GetSdp()
	{
		MSC_TRACE();

		// Increase SDP version.
		auto version = this->sdpObject["origin"]["sessionVersion"].get<uint32_t>();

		this->sdpObject["origin"]["sessionVersion"] = ++version;

		return sdptransform::write(this->sdpObject);
	}

	void Sdp::RemoteSdp::AddMediaSection(std::unique_ptr<MediaSection> newMediaSection)
	{
		MSC_TRACE();

		if (this->firstMid.empty())
		{
			this->firstMid = newMediaSection->GetMid();
		}

		// NOTE: Observer pointer to keep using the media section once the vector owns it.
		const auto* const mediaSection = newMediaSection.get();

		// Add it in the vector.
		this->mediaSections.push_back(std::move(newMediaSection));

		// Add to the map.
		this->midToIndex[mediaSection->GetMid()] = this->mediaSections.size() - 1;

		// Add to the SDP object.
		this->sdpObject["media"].push_back(mediaSection->GetObject());

		RegenerateBundleMids();
	}

	void Sdp::RemoteSdp::ReplaceMediaSection(
	  std::unique_ptr<MediaSection> newMediaSection, const std::string& reuseMid)
	{
		MSC_TRACE();

		// NOTE: Observer pointer to keep using the media section once the vector owns it.
		const auto* const mediaSection = newMediaSection.get();

		// Store it in the map.
		if (!reuseMid.empty())
		{
			const auto idx = this->midToIndex[reuseMid];
			// NOTE: Read before the assignment below, which destroys the old media section.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			const auto oldMid = this->mediaSections[idx]->GetMid();

			// Replace the index in the vector with the new media section.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			this->mediaSections[idx] = std::move(newMediaSection);

			// Update the map.
			this->midToIndex.erase(oldMid);
			this->midToIndex[mediaSection->GetMid()] = idx;

			// Update the SDP object.
			this->sdpObject["media"][idx] = mediaSection->GetObject();

			// Regenerate BUNDLE mids.
			RegenerateBundleMids();
		}
		else
		{
			const auto idx = this->midToIndex[mediaSection->GetMid()];

			// Replace the index in the vector with the new media section.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
			this->mediaSections[idx] = std::move(newMediaSection);

			// Update the SDP object.
			this->sdpObject["media"][this->mediaSections.size() - 1] = mediaSection->GetObject();
		}
	}

	void Sdp::RemoteSdp::RegenerateBundleMids()
	{
		MSC_TRACE();

		std::string mids;

		for (const auto& mediaSection : this->mediaSections)
		{
			if (!mediaSection->IsClosed())
			{
				if (mids.empty())
				{
					mids = mediaSection->GetMid();
				}
				else
				{
					mids.append(" ").append(mediaSection->GetMid());
				}
			}
		}

		this->sdpObject["groups"][0]["mids"] = mids;
	}
} // namespace mediasoupclient
