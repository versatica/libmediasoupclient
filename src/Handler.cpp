#define MSC_CLASS "Handler"
// #define MSC_LOG_DEV

#include "Handler.hpp"
#include "Exception.hpp"
#include "Logger.hpp"
#include "PeerConnection.hpp"
#include "sdp/Utils.hpp"
#include "sdptransform.hpp"
#include <list>
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
/* Handler static methods */

json Handler::GetNativeRtpCapabilities()
{
	MSC_TRACE();

	std::unique_ptr<PeerConnection::PrivateListener> privateListener(
	  new PeerConnection::PrivateListener());
	std::unique_ptr<PeerConnection> pc(new PeerConnection(privateListener.get(), nullptr));

	(void)pc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
	(void)pc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);

	// May throw.
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	auto offer = pc->CreateOffer(options);

	auto sdpObject             = sdptransform::parse(offer);
	auto nativeRtpCapabilities = Sdp::Utils::extractRtpCapabilities(sdpObject);

	return nativeRtpCapabilities;
}

/* Handler instance methods */

Handler::Handler(
  PrivateListener* privateListener,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  const PeerConnection::Options* peerConnectionOptions)
  : privateListener(privateListener)
{
	MSC_TRACE();

	this->pc.reset(new PeerConnection(this, peerConnectionOptions));

	this->remoteSdp.reset(new Sdp::RemoteSdp(iceParameters, iceCandidates, dtlsParameters));
};

void Handler::UpdateIceServers(const json& iceServerUris)
{
	MSC_TRACE();

	auto configuration = this->pc->GetConfiguration();

	for (auto& iceServerUri : iceServerUris)
	{
		webrtc::PeerConnectionInterface::IceServer iceServer;
		iceServer.uri = iceServerUri;
		configuration.servers.push_back(iceServer);
	}

	if (this->pc->SetConfiguration(configuration))
		return;

	throw Exception("UpdateIceServers failed");
};

void Handler::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{
	return this->privateListener->OnConnectionStateChange(newState);
}

void Handler::SetupTransport(const std::string& localDtlsRole, json localSdpObject)
{
	MSC_TRACE();

	if (localSdpObject.empty())
		localSdpObject = sdptransform::parse(this->pc->GetLocalDescription());

	// Get our local DTLS parameters.
	auto dtlsParameters = Sdp::Utils::extractDtlsParameters(localSdpObject);

	// Set our DTLS role.
	dtlsParameters["role"] = localDtlsRole;

	std::string remoteDtlsRole = localDtlsRole == "client" ? "server" : "client";
	this->remoteSdp->UpdateDtlsRole(remoteDtlsRole);

	// May throw.
	this->privateListener->OnConnect(dtlsParameters);
	this->transportReady = true;
};

/* SendHandler methods */

SendHandler::SendHandler(
  Handler::PrivateListener* privateListener,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  const PeerConnection::Options* peerConnectionOptions,
  const json& sendingRtpParametersByKind,
  const json& sendingRemoteRtpParametersByKind)
  : Handler(privateListener, iceParameters, iceCandidates, dtlsParameters, peerConnectionOptions)
{
	MSC_TRACE();

	this->sendingRtpParametersByKind = sendingRtpParametersByKind;

	this->sendingRemoteRtpParametersByKind = sendingRemoteRtpParametersByKind;
};

std::pair<std::string, nlohmann::json> SendHandler::Send(
  webrtc::MediaStreamTrackInterface* track,
  const std::vector<webrtc::RtpEncodingParameters>* encodings,
  const json* codecOptions)
{
	MSC_TRACE();

	// Check if the track is a null pointer.
	if (track == nullptr)
		throw Exception("Track cannot be null");

	MSC_DEBUG("[kind:%s, track.id:%s]", track->kind().c_str(), track->id().c_str());

	// https://bugs.chromium.org/p/webrtc/issues/detail?id=7600
	// Once the issue is solved, no SDP will be required to enable simulcast.
	webrtc::RtpTransceiverInterface* transceiver = this->pc->AddTransceiver(track);

	if (transceiver == nullptr)
		throw Exception("Error creating transceiver");

	transceiver->SetDirection(webrtc::RtpTransceiverDirection::kSendOnly);

	std::string offer;
	std::string localId;
	json& sendingRtpParameters = this->sendingRtpParametersByKind[track->kind()];

	try
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		offer               = this->pc->CreateOffer(options);
		auto localSdpObject = sdptransform::parse(offer);

		// Transport is not ready.
		if (!this->transportReady)
			this->SetupTransport("server", localSdpObject);

		if (encodings != nullptr && encodings->size() > 1)
		{
			MSC_DEBUG("enabling legacy simulcast");

			// We know that our media section is the last one.
			auto numMediaSection   = localSdpObject["media"].size();
			json& offerMediaObject = localSdpObject["media"][numMediaSection - 1];

			Sdp::Utils::addLegacySimulcast(offerMediaObject, encodings->size());

			offer = sdptransform::write(localSdpObject);
		}

		MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer);

		// We can now get the transceiver.mid.
		localId = transceiver->mid().value();

		// Set MID.
		sendingRtpParameters["mid"] = localId;
	}
	catch (Exception& error)
	{
		// Panic here. Try to undo things.
		transceiver->SetDirection(webrtc::RtpTransceiverDirection::kInactive);
		transceiver->sender()->SetTrack(nullptr);

		throw;
	}

	auto localSdp       = this->pc->GetLocalDescription();
	auto localSdpObject = sdptransform::parse(localSdp);

	// We know that our media section is the last one.
	auto numMediaSection   = localSdpObject["media"].size();
	json& offerMediaObject = localSdpObject["media"][numMediaSection - 1];

	// Set RTCP CNAME.
	try
	{
		sendingRtpParameters["rtcp"]["cname"] = Sdp::Utils::getCname(offerMediaObject);

		// Set RTP encodings.
		sendingRtpParameters["encodings"] = Sdp::Utils::getRtpEncodings(offerMediaObject);

		// If VP8 and there is effective simulcast, add scalabilityMode to each encoding.
		auto mimeType = sendingRtpParameters["codecs"][0]["mimeType"].get<std::string>();

		std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

		// clang-format off
		if (
			sendingRtpParameters["encodings"].size() > 1 &&
			(mimeType == "video/vp8" || mimeType == "video/h264")
		)
		// clang-format on
		{
			for (auto& encoding : sendingRtpParameters["encodings"])
			{
				encoding["scalabilityMode"] = "S1T3";
			}
		}

		this->remoteSdp->Send(
		  offerMediaObject,
		  sendingRtpParameters,
		  this->sendingRemoteRtpParametersByKind[track->kind()],
		  codecOptions);

		auto answer = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

		this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer);
	}
	catch (Exception& error)
	{
		throw;
	}

	// Store in the map.
	this->mapMidTransceiver[localId] = transceiver;

	return std::make_pair(localId, sendingRtpParameters);
}

void SendHandler::StopSending(const std::string& localId)
{
	MSC_TRACE();

	MSC_DEBUG("[localId:%s]", localId.c_str());

	auto jsonLocaIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocaIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocaIdIt->second;

	transceiver->sender()->SetTrack(nullptr);
	this->pc->RemoveTrack(transceiver->sender());
	this->remoteSdp->DisableMediaSection(transceiver->mid().value());

	// May throw.
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

	auto offer = this->pc->CreateOffer(options);

	MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

	// May throw.
	this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer);

	auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
	auto answer      = this->remoteSdp->GetSdp();

	MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

	// May throw.
	this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer);
}

void SendHandler::ReplaceTrack(const std::string& localId, webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	MSC_DEBUG(
	  "[localId:%s, track->id():%s]",
	  localId.c_str(),
	  track == nullptr ? "nullptr" : track->id().c_str());

	auto jsonLocalIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocalIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocalIdIt->second;

	transceiver->sender()->SetTrack(track);
}

void SendHandler::SetMaxSpatialLayer(const std::string& localId, uint8_t spatialLayer)
{
	MSC_TRACE();

	MSC_DEBUG("[localId:%s, spatialLayer:%d]", localId.c_str(), spatialLayer);

	auto jsonLocalIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocalIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocalIdIt->second;

	auto parameters = transceiver->sender()->GetParameters();

	bool hasLowEncoding{ false }, hasMediumEncoding{ false }, hasHighEncoding{ false };
	webrtc::RtpEncodingParameters *lowEncoding, *mediumEncoding, *highEncoding;

	if (!parameters.encodings.empty())
	{
		hasLowEncoding = true;
		lowEncoding    = &parameters.encodings[0];
	}

	if (parameters.encodings.size() > 1)
	{
		hasMediumEncoding = true;
		mediumEncoding    = &parameters.encodings[1];
	}

	if (parameters.encodings.size() > 2)
	{
		hasHighEncoding = true;
		highEncoding    = &parameters.encodings[2];
	}

	// Edit encodings.
	if (spatialLayer == 1u)
	{
		hasLowEncoding && (lowEncoding->active = true);
		hasMediumEncoding && (mediumEncoding->active = false);
		hasHighEncoding && (highEncoding->active = false);
	}

	else if (spatialLayer == 2u)
	{
		hasLowEncoding && (lowEncoding->active = true);
		hasMediumEncoding && (mediumEncoding->active = true);
		hasHighEncoding && (highEncoding->active = false);
	}

	else if (spatialLayer == 3u)
	{
		hasLowEncoding && (lowEncoding->active = true);
		hasMediumEncoding && (mediumEncoding->active = true);
		hasHighEncoding && (highEncoding->active = true);
	}

	auto result = transceiver->sender()->SetParameters(parameters);
	if (!result.ok())
		throw Exception(result.message());
}

json SendHandler::GetSenderStats(const std::string& localId)
{
	MSC_TRACE();

	MSC_DEBUG("[localId:%s]", localId.c_str());

	auto jsonLocalIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocalIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocalIdIt->second;

	auto stats = this->pc->GetStats(transceiver->sender());

	return stats;
}

void SendHandler::RestartIce(const json& iceParameters)
{
	MSC_TRACE();

	// Provide the remote SDP handler with new remote ICE parameters.
	this->remoteSdp->UpdateIceParameters(iceParameters);

	if (!this->transportReady)
		return;

	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	options.ice_restart = true;

	// May throw.
	auto offer = this->pc->CreateOffer(options);

	MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

	// May throw.
	this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer);

	auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
	auto answer      = this->remoteSdp->GetSdp();

	MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

	// May throw.
	this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer);
}

/* RecvHandler methods */

RecvHandler::RecvHandler(
  Handler::PrivateListener* privateListener,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  const PeerConnection::Options* peerConnectionOptions)
  : Handler(privateListener, iceParameters, iceCandidates, dtlsParameters, peerConnectionOptions)
{
	MSC_TRACE();
};

std::pair<std::string, webrtc::MediaStreamTrackInterface*> RecvHandler::Receive(
  const std::string& id, const std::string& kind, const json* rtpParameters)
{
	MSC_TRACE();

	MSC_DEBUG("[id:%s, kind:%s]", id.c_str(), kind.c_str());

	auto localId = std::to_string(this->nextMid);

	auto encoding = (*rtpParameters)["encodings"][0];
	auto cname    = (*rtpParameters)["rtcp"]["cname"];

	this->remoteSdp->Receive(localId, kind, *rtpParameters, cname, id);

	auto offer = this->remoteSdp->GetSdp();

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	try
	{
		// May throw.
		this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer);

		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		// May throw.
		auto answer = this->pc->CreateAnswer(options);

		auto localSdpObject = sdptransform::parse(answer);
		auto jsonMediaIt    = find_if(
      localSdpObject["media"].begin(), localSdpObject["media"].end(), [&localId](const json& m) {
        return m["mid"].get<std::string>() == localId;
      });

		auto answerMediaObject = *jsonMediaIt;

		// May need to modify codec parameters in the answer based on codec
		// parameters in the offer.
		Sdp::Utils::applyCodecParameters(*rtpParameters, answerMediaObject);

		answer = sdptransform::write(localSdpObject);

		if (!this->transportReady)
			this->SetupTransport("client", localSdpObject);

		MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

		// May throw.
		this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer);
	}
	catch (Exception& error)
	{
		throw;
	}

	auto transceivers  = this->pc->GetTransceivers();
	auto transceiverIt = std::find_if(
	  transceivers.begin(), transceivers.end(), [&localId](webrtc::RtpTransceiverInterface* t) {
		  return t->mid() == localId;
	  });

	if (transceiverIt == transceivers.end())
		throw Exception("new RTCRtpTransceiver not found");

	auto transceiver = *transceiverIt;

	// Store in the map.
	this->mapMidTransceiver[localId] = transceiver;

	// Increase next MID.
	this->nextMid++;

	return std::make_pair(localId, transceiver->receiver()->track());
}

void RecvHandler::StopReceiving(const std::string& localId)
{
	MSC_TRACE();

	MSC_DEBUG("[localId:%s]", localId.c_str());

	auto jsonLocalIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocalIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocalIdIt->second;
	MSC_DEBUG("--- disabling mid: %s", transceiver->mid().value().c_str());
	this->remoteSdp->DisableMediaSection(transceiver->mid().value());

	auto offer = this->remoteSdp->GetSdp();

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	// May throw.
	this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer);

	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	// May throw.
	auto answer = this->pc->CreateAnswer(options);

	MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

	// May throw.
	this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer);
}

json RecvHandler::GetReceiverStats(const std::string& localId)
{
	MSC_TRACE();

	MSC_DEBUG("[localId:%s]", localId.c_str());

	MSC_DEBUG("[localId:%s]", localId.c_str());

	auto jsonLocalIdIt = this->mapMidTransceiver.find(localId);
	if (jsonLocalIdIt == this->mapMidTransceiver.end())
		throw Exception("Associated RtpTransceiver not found");

	auto transceiver = jsonLocalIdIt->second;

	// May throw.
	auto stats = this->pc->GetStats(transceiver->receiver());

	return stats;
}

void RecvHandler::RestartIce(const json& iceParameters)
{
	MSC_TRACE();

	// Provide the remote SDP handler with new remote ICE parameters.
	this->remoteSdp->UpdateIceParameters(iceParameters);

	if (!this->transportReady)
		return;

	auto offer = this->remoteSdp->GetSdp();

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	// May throw.
	this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer);

	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	// May throw.
	auto answer = this->pc->CreateAnswer(options);

	MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

	// May throw.
	this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer);
}
} // namespace mediasoupclient
