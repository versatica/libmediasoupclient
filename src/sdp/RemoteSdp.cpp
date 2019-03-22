#define MSC_CLASS "Sdp::RemoteSdp"
// #define MSC_LOG_DEV

#include "sdp/RemoteSdp.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "sdptransform.hpp"
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
/* Sdp::RemoteSdp methods */

Sdp::RemoteSdp::RemoteSdp(const json& iceParameters, const json& iceCandidates, const json& dtlsParameters)
  : iceParameters(iceParameters), iceCandidates(iceCandidates), dtlsParameters(dtlsParameters)
{
	MSC_TRACE();

	/* clang-format off */
	this->sdpObject =
	{
		{ "id",      Utils::getRandomInteger(1000, 1000000) },
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
	/* clang-format on */

	// If ICE parameters are given, add ICE-Lite indicator.
	if (this->iceParameters.find("iceLite") != this->iceParameters.end())
		this->sdpObject["icelite"] = "ice-lite";

	/* clang-format off */
	this->sdpObject["msidSemantic"] =
	{
		{ "semantic", "WMS" },
		{ "token",    "*"   }
	};
	/* clang-format on */

	// NOTE: We take the latest fingerprint.
	auto numFingerprints = this->dtlsParameters["fingerprints"].size();

	this->sdpObject["fingerprint"] = {
		{ "type", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["algorithm"] },
		{ "hash", this->dtlsParameters.at("fingerprints")[numFingerprints - 1]["value"] }
	};

	/* clang-format off */
	this->sdpObject["groups"] =
	{
		{
			{ "type", "BUNDLE" },
			{ "mids", ""       }
		}
	};
	/* clang-format on */
}

void Sdp::RemoteSdp::UpdateIceParameters(const json& iceParameters)
{
	MSC_TRACE();

	this->iceParameters = iceParameters;

	if (iceParameters.find("iceLite") != iceParameters.end())
		sdpObject["icelite"] = "ice-lite";

	for (auto& kv : this->mediaSections)
	{
		auto mediaSection = kv.second;

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
		auto mediaSection = kv.second;

		mediaSection->SetDtlsRole(role);
	}
}

void Sdp::RemoteSdp::Send(
  nlohmann::json& offerMediaObject,
  nlohmann::json& offerRtpParameters,
  nlohmann::json& answerRtpParameters,
  const nlohmann::json* codecOptions)
{
	MSC_TRACE();

	auto mediaSection = new AnswerMediaSection(
	  this->iceParameters,
	  this->iceCandidates,
	  this->dtlsParameters,
	  offerMediaObject,
	  offerRtpParameters,
	  answerRtpParameters,
	  codecOptions);

	this->AddMediaSection(mediaSection);
}

void Sdp::RemoteSdp::Receive(
  const std::string& mid,
  const std::string& kind,
  const nlohmann::json& offerRtpParameters,
  const std::string& streamId,
  const std::string& trackId)
{
	MSC_TRACE();

	auto mediaSection = new OfferMediaSection(
	  this->iceParameters,
	  this->iceCandidates,
	  this->dtlsParameters,
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

	auto mediaSection = this->mediaSections[mid];

	mediaSection->Disable();
}

std::string Sdp::RemoteSdp::GetSdp()
{
	MSC_TRACE();

	// Increase SDP version.
	auto version = this->sdpObject["origin"]["sessionVersion"].get<uint32_t>();
	this->sdpObject["origin"]["sessionVersion"] = ++version;

	return sdptransform::write(this->sdpObject);
}

void Sdp::RemoteSdp::AddMediaSection(MediaSection* mediaSection)
{
	MSC_TRACE();

	// Store it in the map.
	this->mediaSections[mediaSection->GetMid()] = mediaSection;

	// Update SDP object.
	this->sdpObject["media"].push_back(mediaSection->GetObject());

	std::string mids = this->sdpObject["groups"][0]["mids"].get<std::string>();
	if (mids.empty())
		mids = mediaSection->GetMid();
	else
		mids.append(" ").append(mediaSection->GetMid());

	this->sdpObject["groups"][0]["mids"] = mids;
}
} // namespace mediasoupclient
