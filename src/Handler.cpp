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

/* Handler static methods */

/*
 * Get PeerConnection RTP capabilities.
 */
json Handler::GetNativeRtpCapabilities()
{
	std::unique_ptr<PeerConnection> pc(new PeerConnection(nullptr, {}));

	return pc->GetNativeRtpCapabilities();
}

/*
 * Get PeerConnection name
 */
const std::string& Handler::GetName()
{
	return PeerConnection::GetName();
}

/* Handler instance methods */

Handler::Handler(
  Listener* listener,
  const json& iceServers,
  const std::string& /*iceTransportPolicy*/,
  const json& /*proprietaryConstraints*/,
  json sendingRtpParametersByKind)
  : listener(listener), sendingRtpParametersByKind(std::move(sendingRtpParametersByKind))
{
	MSC_TRACE();

	this->pc.reset(new PeerConnection(
	  this, iceServers.get<std::list<std::string>>()
	  // iceTransportPolicy,
	  /* The following must go into PeerConnection directly
	     'max-bundle',
	     'require',
	     */
	  // proprietaryConstraints
	  ));
};

std::future<void> Handler::UpdateIceServers(const json& iceServerUris)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));
	};

	auto configuration = this->pc->GetConfiguration();

	for (auto& iceServerUri : iceServerUris)
	{
		webrtc::PeerConnectionInterface::IceServer iceServer;
		iceServer.uri = iceServerUri;
		configuration.servers.push_back(iceServer);
	}

	if (this->pc->SetConfiguration(configuration))
		promise.set_value();
	else
		reject(Exception("UpdateIceServers failed"));

	return promise.get_future();
};

void Handler::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{
	return this->listener->OnConnectionStateChange(newState);
}

void Handler::SetupTransport(const std::string& localDtlsRole)
{
	MSC_TRACE();

	// Get our local DTLS parameters.
	auto sdp            = this->pc->GetLocalDescription();
	auto sdpObj         = sdptransform::parse(sdp);
	auto dtlsParameters = Sdp::Utils::extractDtlsParameters(sdpObj);

	// Set our DTLS role.
	dtlsParameters["role"] = localDtlsRole;

	json transportLocalParameters = { { "dtlsParameters", dtlsParameters } };

	try
	{
		this->listener->OnConnect(transportLocalParameters).get();

		// TODO: this should mean the the server has created the corresponding transport.
		this->transportReady = true;
	}
	catch (Exception& error)
	{
	}
};

/* SendHandler methods */

SendHandler::SendHandler(
  Handler::Listener* listener,
  const json& transportRemoteParameters,
  const json& iceServers,
  const std::string& iceTransportPolicy,
  const json& proprietaryConstraints,
  const json& rtpParametersByKind)
  : ::Handler(listener, iceServers, iceTransportPolicy, proprietaryConstraints, rtpParametersByKind)
{
	MSC_TRACE();

	this->remoteSdp.reset(new Sdp::RemoteSdp(transportRemoteParameters, rtpParametersByKind));
};

std::future<json> SendHandler::Send(webrtc::MediaStreamTrackInterface* track, uint8_t numStreams)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<json> promise;

	webrtc::RtpTransceiverInterface* transceiver{ nullptr };

	auto reject = [&transceiver, &promise](const Exception& error) {
		// Panic here. Try to undo things.
		if (transceiver != nullptr)
			transceiver->SetDirection(webrtc::RtpTransceiverDirection::kInactive);

		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));

	MSC_DEBUG("[kind:%s, track.id:%s]", track->kind().c_str(), track->id().c_str());

	// Check if the track is alrady handled.
	if (this->tracks.find(track) != this->tracks.end())
		return reject(Exception("Track already handled"));

	// Check if there is any inactive transceiver for same kind and reuse it if so.
	auto transceivers = this->pc->GetTransceivers();
	auto it           = std::find_if(
    transceivers.begin(), transceivers.end(), [&track](webrtc::RtpTransceiverInterface* transceiver) {
      return (
        transceiver->receiver()->track()->kind() == track->kind() &&
        transceiver->direction() == webrtc::RtpTransceiverDirection::kInactive);
    });

	if (it != transceivers.end())
	{
		MSC_DEBUG("reusing an inactive transceiver");

		transceiver = *it;
		transceiver->sender()->SetTrack(track);
	}
	else
	{
		// https://bugs.chromium.org/p/webrtc/issues/detail?id=7600
		// Once the issue is solved, no SDP will be required to enable simulcast.
		transceiver = this->pc->AddTransceiver(track);

		if (transceiver == nullptr)
			return reject(Exception("Error creating transceiver"));
	}

	transceiver->SetDirection(webrtc::RtpTransceiverDirection::kSendOnly);

	std::string offer;

	try
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		offer = this->pc->CreateOffer(options).get();

		if (numStreams > 1)
		{
			MSC_DEBUG("enabling simulcast");

			auto sdpObject = sdptransform::parse(offer);

			Sdp::Utils::addLegacySimulcast(sdpObject, track, numStreams);

			offer = sdptransform::write(sdpObject);
		}

		MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	// Transport is not ready.
	if (!this->transportReady)
		this->SetupTransport("server");

	auto localSdp    = this->pc->GetLocalDescription();
	auto localSdpObj = sdptransform::parse(localSdp);

	try
	{
		auto answer = this->remoteSdp->CreateAnswerSdp(localSdpObj);

		MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

		this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	auto rtpParameters = this->sendingRtpParametersByKind[track->kind()];
	Sdp::Utils::fillRtpParametersForTrack(rtpParameters, localSdpObj, transceiver->mid().value());

	this->tracks.insert(track);

	promise.set_value(rtpParameters);

	return promise.get_future();
}

std::future<void> SendHandler::StopSending(webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));

	MSC_DEBUG("[track->id():%s]", track->id().c_str());

	auto senders = this->pc->GetSenders();
	auto it =
	  std::find_if(senders.begin(), senders.end(), [&track](const webrtc::RtpSenderInterface* s) {
		  return s->track() == track;
	  });

	if (it == senders.end())
		return reject(Exception("Local track not found"));

	this->pc->RemoveTrack(*it);

	try
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

		auto offer = this->pc->CreateOffer(options).get();

		MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
	auto answer      = this->remoteSdp->CreateAnswerSdp(localSdpObj);

	MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

	this->tracks.erase(track);

	try
	{
		this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	promise.set_value();

	return promise.get_future();
}

std::future<void> SendHandler::ReplaceTrack(
  webrtc::MediaStreamTrackInterface* track, webrtc::MediaStreamTrackInterface* newTrack)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));

	MSC_DEBUG("[newTrack->id():%s]", newTrack == nullptr ? "nullptr" : newTrack->id().c_str());

	if (this->tracks.find(newTrack) != this->tracks.end())
		return reject(Exception("Track already handled"));

	// Get the associated RtpSender.
	auto senders = this->pc->GetSenders();
	auto it =
	  std::find_if(senders.begin(), senders.end(), [&track](const webrtc::RtpSenderInterface* s) {
		  return s->track() == track;
	  });

	if (it == senders.end())
		return reject(Exception("Local track not found"));

	auto sender = *it;
	if (!sender->SetTrack(newTrack))
		return reject(Exception("Error setting new track"));

	this->tracks.erase(track);

	if (newTrack != nullptr)
		this->tracks.insert(newTrack);

	promise.set_value();

	return promise.get_future();
}

std::future<void> SendHandler::SetMaxSpatialLayer(
  webrtc::MediaStreamTrackInterface* track, const std::string& spatialLayer)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));

	MSC_DEBUG("[track->id():%s, spatialLayer:%s]", track->id().c_str(), spatialLayer.c_str());

	// Get the associated RtpSender.
	auto senders = this->pc->GetSenders();
	auto it =
	  std::find_if(senders.begin(), senders.end(), [&track](const webrtc::RtpSenderInterface* s) {
		  return s->track() == track;
	  });

	if (it == senders.end())
		return reject(Exception("Local track not found"));

	auto rtpSender = *it;

	auto parameters = rtpSender->GetParameters();

	bool hasLowEncoding{ false }, hasMediumEncoding{ false }, hasHighEncoding{ false };
	webrtc::RtpEncodingParameters lowEncoding, mediumEncoding, highEncoding;

	if (!parameters.encodings.empty())
	{
		hasLowEncoding = true;
		lowEncoding    = parameters.encodings[0];
	}

	if (parameters.encodings.size() > 1)
	{
		hasMediumEncoding = true;
		mediumEncoding    = parameters.encodings[1];
	}

	if (parameters.encodings.size() > 2)
	{
		hasHighEncoding = true;
		highEncoding    = parameters.encodings[2];
	}

	// Edit encodings.
	if (spatialLayer == "low")
	{
		hasLowEncoding && (lowEncoding.active = true);
		hasMediumEncoding && (mediumEncoding.active = false);
		hasHighEncoding && (highEncoding.active = false);
	}

	else if (spatialLayer == "medium")
	{
		hasLowEncoding && (lowEncoding.active = true);
		hasMediumEncoding && (mediumEncoding.active = true);
		hasHighEncoding && (highEncoding.active = false);
	}

	else if (spatialLayer == "high")
	{
		hasLowEncoding && (lowEncoding.active = true);
		hasMediumEncoding && (mediumEncoding.active = true);
		hasHighEncoding && (highEncoding.active = true);
	}

	auto result = rtpSender->SetParameters(parameters);
	if (!result.ok())
		return reject(Exception(result.message()));

	promise.set_value();

	return promise.get_future();
}

std::future<json> SendHandler::GetSenderStats(webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<json> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));

	MSC_DEBUG("[track->id():%s]", track->id().c_str());

	// Get the associated RtpSender.
	auto senders = this->pc->GetSenders();
	auto it =
	  std::find_if(senders.begin(), senders.end(), [&track](const webrtc::RtpSenderInterface* s) {
		  return s->track() == track;
	  });

	if (it == senders.end())
		return reject(Exception("Local track not found"));

	auto rtpSender = *it;

	auto stats = this->pc->GetStats(rtpSender).get();

	promise.set_value(stats);

	return promise.get_future();
}

std::future<void> SendHandler::RestartIce(const json& remoteIceParameters)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Provide the remote SDP handler with new remote ICE parameters.
	this->remoteSdp->UpdateTransportRemoteIceParameters(remoteIceParameters);

	if (!this->transportReady)
	{
		promise.set_value();

		return promise.get_future();
	}

	try
	{
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		options.ice_restart = true;

		auto offer = this->pc->CreateOffer(options).get();

		MSC_DEBUG("calling pc->SetLocalDescription() [offer:%s]", offer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::OFFER, offer).get();

		auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
		auto answer      = this->remoteSdp->CreateAnswerSdp(localSdpObj);

		MSC_DEBUG("calling pc->SetRemoteDescription() [answer:%s]", answer.c_str());

		this->pc->SetRemoteDescription(PeerConnection::SdpType::ANSWER, answer);
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	promise.set_value();

	return promise.get_future();
}

/* RecvHandler methods */

RecvHandler::RecvHandler(
  Handler::Listener* listener,
  const json& transportRemoteParameters,
  const json& iceServers,
  const std::string& iceTransportPolicy,
  const json& proprietaryConstraints)
  : ::Handler(listener, iceServers, iceTransportPolicy, proprietaryConstraints)
{
	MSC_TRACE();

	this->remoteSdp.reset(new Sdp::RemoteSdp(transportRemoteParameters));
};

std::future<webrtc::MediaStreamTrackInterface*> RecvHandler::Receive(
  const std::string& id, const std::string& kind, const json& rtpParameters)
{
	MSC_TRACE();

	MSC_DEBUG("[id:%s, kind:%s]", id.c_str(), kind.c_str());

	// Returned future's promise.
	std::promise<webrtc::MediaStreamTrackInterface*> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	if (this->receiverInfos.find(id) != this->receiverInfos.end())
		return reject(Exception("Already receiving this source"));

	auto encoding = rtpParameters["encodings"][0];
	auto cname    = rtpParameters["rtcp"]["cname"];

	std::string mid(&kind[0], 1);
	mid.append("-").append(id);

	std::string trackId(kind);
	trackId.append("-").append(id);

	/* clang-format off */
	json receiverInfo =
	{
		{ "mid",           mid           },
		{ "kind",          kind          },
		{ "closed",        false         },
		{ "streamId",      id            },
		{ "trackId",       trackId       },
		{ "rtpParameters", rtpParameters }
	};
	/* clang-format on */

	auto it = encoding.find("rtx");
	if (it != encoding.end())
		receiverInfo["rtxSsrc"] = encoding["rtx"]["ssrc"];

	this->receiverInfos[id] = receiverInfo;

	auto receiverInfos = json::array();
	std::for_each(
	  this->receiverInfos.begin(),
	  this->receiverInfos.end(),
	  [&receiverInfos](std::pair<const std::string, json>& kv) { receiverInfos.push_back(kv.second); });

	auto offer = this->remoteSdp->CreateOfferSdp(receiverInfos);

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	try
	{
		this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer).get();

		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		auto answer = this->pc->CreateAnswer(options).get();

		MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer).get();
	}
	catch (Exception& error)
	{
		this->receiverInfos.erase(id);

		return reject(error);
	}

	if (!this->transportReady)
		this->SetupTransport("client");

	auto transceivers  = this->pc->GetTransceivers();
	auto transceiverIt = std::find_if(
	  transceivers.begin(), transceivers.end(), [&mid](webrtc::RtpTransceiverInterface* t) {
		  return t->mid() == mid;
	  });

	if (transceiverIt == transceivers.end())
		return reject(Exception("Remote track not found"));

	auto transceiver = *transceiverIt;

	promise.set_value(transceiver->receiver()->track());

	return promise.get_future();
}

std::future<void> RecvHandler::StopReceiving(const std::string& id)
{
	MSC_TRACE();

	MSC_DEBUG("[id:%s]", id.c_str());

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	auto it = this->receiverInfos.find(id);
	if (it == this->receiverInfos.end())
		return reject(Exception("Receiver not found"));

	auto receiverInfo      = it->second;
	receiverInfo["closed"] = true;

	auto receiverInfos = json::array();
	std::for_each(
	  this->receiverInfos.begin(),
	  this->receiverInfos.end(),
	  [&receiverInfos](std::pair<const std::string, json>& kv) { receiverInfos.push_back(kv.second); });

	auto offer = this->remoteSdp->CreateOfferSdp(receiverInfos);

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	try
	{
		this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer).get();

		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		auto answer = this->pc->CreateAnswer(options).get();

		MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	promise.set_value();

	return promise.get_future();
}

std::future<json> RecvHandler::GetReceiverStats(const std::string& id)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<json> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	MSC_DEBUG("[id:%s]", id.c_str());

	auto it = this->receiverInfos.find(id);
	if (it == this->receiverInfos.end())
		return reject(Exception("Receiver not found"));

	auto receiverInfo = it->second;
	auto mid          = receiverInfo["mid"].get<std::string>();

	// Get the associated RtpReceiver.
	auto transceivers  = this->pc->GetTransceivers();
	auto transceiverIt = std::find_if(
	  transceivers.begin(), transceivers.end(), [&mid](const webrtc::RtpTransceiverInterface* t) {
		  return t->mid() == mid;
	  });

	if (transceiverIt == transceivers.end())
		return reject(Exception("Transceiver not found"));

	auto transceiver = *transceiverIt;
	auto rtpReceiver = transceiver->receiver();

	auto stats = this->pc->GetStats(rtpReceiver).get();

	promise.set_value(stats);

	return promise.get_future();
}

std::future<void> RecvHandler::RestartIce(const json& remoteIceParameters)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Provide the remote SDP handler with new remote ICE parameters.
	this->remoteSdp->UpdateTransportRemoteIceParameters(remoteIceParameters);

	if (!this->transportReady)
	{
		promise.set_value();

		return promise.get_future();
	}

	auto receiverInfos = json::array();
	std::for_each(
	  this->receiverInfos.begin(),
	  this->receiverInfos.end(),
	  [&receiverInfos](std::pair<const std::string, json>& kv) { receiverInfos.push_back(kv.second); });

	auto offer = this->remoteSdp->CreateOfferSdp(receiverInfos);

	MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", offer.c_str());

	try
	{
		this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer).get();

		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		auto answer = this->pc->CreateAnswer(options).get();

		MSC_DEBUG("calling pc->SetLocalDescription() [answer:%s]", answer.c_str());

		this->pc->SetLocalDescription(PeerConnection::SdpType::ANSWER, answer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	promise.set_value();

	return promise.get_future();
}
