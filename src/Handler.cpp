#define MSC_CLASS "Handler"

#include "Handler.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include "PeerConnection.hpp"
#include "ortc.hpp"
#include "scalabilityMode.hpp"
#include "sdptransform.hpp"
#include "sdp/Utils.hpp"
#include <cinttypes> // PRIu64, etc

using json = nlohmann::json;

constexpr uint16_t SctpNumStreamsOs{ 1024u };
constexpr uint16_t SctpNumStreamsMis{ 1024u };

json SctpNumStreams = { { "OS", SctpNumStreamsOs }, { "MIS", SctpNumStreamsMis } };

// Static functions declaration.
static void fillJsonRtpEncodingParameters(
  json& jsonEncoding, const webrtc::RtpEncodingParameters& encoding);

namespace mediasoupclient
{
	// Shims (to be removed)
	namespace {
		std::string CreateAnswer(PeerConnection& pc, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& opts = {})
		{
			MSC_TRACE();

			std::promise<std::string> promise;

			pc.CreateAnswer(opts, [&promise](auto sdp, auto err) {
				if (err.ok()) {
					promise.set_value(sdp);
				} else {
					promise.set_exception(std::make_exception_ptr(MediaSoupClientError(err.message())));
				}
			});

			return promise.get_future().get();
		}

		std::string CreateOffer(PeerConnection& pc, const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& opts = {})
		{
			MSC_TRACE();

			std::promise<std::string> promise;

			pc.CreateOffer(opts, [&promise](auto sdp, auto err) {
				if (err.ok()) {
					promise.set_value(sdp);
				} else {
					promise.set_exception(std::make_exception_ptr(MediaSoupClientError(err.message())));
				}
			});

			return promise.get_future().get();
		}

		void SetLocalDescription(PeerConnection& pc, const PeerConnection::SdpType& type, const std::string& sdp)
		{
			MSC_TRACE();

			std::promise<void> promise;

			pc.SetLocalDescription(type, sdp, [&promise](auto sdp, auto err) {
				if (err.ok()) {
					promise.set_value();
				} else {
					promise.set_exception(std::make_exception_ptr(MediaSoupClientError(err.message())));
				}
			});

			return promise.get_future().get();
		}

		void SetRemoteDescription(PeerConnection& pc, const PeerConnection::SdpType& type, const std::string& sdp)
		{
			MSC_TRACE();

			std::promise<void> promise;

			pc.SetRemoteDescription(type, sdp, [&promise](auto sdp, auto err) {
				if (err.ok()) {
					promise.set_value();
				} else {
					promise.set_exception(std::make_exception_ptr(MediaSoupClientError(err.message())));
				}
			});

			return promise.get_future().get();
		}
	}

	/* Handler static methods. */

	void Handler::GetNativeRtpCapabilities(const PeerConnection::Options& peerConnectionOptions, CapabilityCallback callback)
	{
		MSC_TRACE();

		auto listener = new PeerConnection::PrivateListener();
		auto pc = new PeerConnection(listener, peerConnectionOptions);

		(void)pc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
		(void)pc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);

		pc->CreateOffer([=](auto offer, auto error) {
			json capabilities;

			if (error.ok()) {
				auto sdpObject = sdptransform::parse(offer);
				capabilities = Sdp::Utils::extractRtpCapabilities(sdpObject);
			}

			delete pc;
			delete listener;

			callback(capabilities, error);
		});
	}

	json Handler::GetNativeSctpCapabilities()
	{
		MSC_TRACE();

		json caps = { { "numStreams", SctpNumStreams } };

		return caps;
	}

	/* Handler instance methods. */

	Handler::Handler(
	  PrivateListener* privateListener,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
	  const PeerConnection::Options& peerConnectionOptions)
	  : privateListener(privateListener)
	{
		MSC_TRACE();

		if (dtlsParameters.find("role") != dtlsParameters.end() && dtlsParameters["role"].get<std::string>() != "auto")
		{
			this->forcedLocalDtlsRole =
			  dtlsParameters["role"].get<std::string>() == "server" ? "client" : "server";
		}

		this->pc.reset(new PeerConnection(this, peerConnectionOptions));

		this->remoteSdp.reset(
		  new Sdp::RemoteSdp(iceParameters, iceCandidates, dtlsParameters, sctpParameters));
	};

	void Handler::Close()
	{
		MSC_TRACE();

		// Clear the stored transceivers before closing the PeerConnection.
		this->mapMidTransceiver.clear();

		this->pc->Close();
	};

	void Handler::GetTransportStats(PeerConnection::StatsHandler callback)
	{
		MSC_TRACE();

		pc->GetStats(callback);
	}

	void Handler::UpdateIceServers(const json& iceServerUris)
	{
		MSC_TRACE();

		auto configuration = this->pc->GetConfiguration();

		configuration.servers.clear();

		for (const auto& iceServerUri : iceServerUris)
		{
			webrtc::PeerConnectionInterface::IceServer iceServer;

			iceServer.uri = iceServerUri.get<std::string>();
			configuration.servers.push_back(iceServer);
		}

		if (this->pc->SetConfiguration(configuration))
			return;

		MSC_THROW_ERROR("failed to update ICE servers");
	};

	void Handler::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
	{
		MSC_TRACE();

		return this->privateListener->OnConnectionStateChange(newState);
	}

	static json DtlsParameters(const std::string& localDtlsRole, const json& localSdpObject) {
		auto dtlsParameters = Sdp::Utils::extractDtlsParameters(localSdpObject);
		dtlsParameters["role"] = localDtlsRole;
		return dtlsParameters;
	}

	std::string Handler::LocalDtlsRole() {
		return !this->forcedLocalDtlsRole.empty() ? this->forcedLocalDtlsRole : this->DefaultLocalDtlsRole();
	}

	void Handler::GetDtlsParameters(DtlsParametersCallback callback)
	{
		this->CreateLocalSDP([callback, role = LocalDtlsRole()](auto sdp, auto error) mutable {
			json dtlsParameters = nullptr;

			if (error.ok()) {
				dtlsParameters = DtlsParameters(role, sdptransform::parse(sdp));
			}

			callback(dtlsParameters, error);
		});
	}

	void Handler::SetupTransport(const json& localSdpObject)
	{
		MSC_TRACE();

		assert(!localSdpObject.empty());

		// Get our local DTLS parameters.
		auto dtlsParameters = DtlsParameters(LocalDtlsRole(), localSdpObject);

		// Update the remote DTLS role in the SDP.
		std::string remoteDtlsRole = LocalDtlsRole() == "client" ? "server" : "client";
		this->remoteSdp->UpdateDtlsRole(remoteDtlsRole);

		// May throw.
		this->privateListener->OnConnect(dtlsParameters);
		this->transportReady = true;
	};

	/* SendHandler instance methods. */

	SendHandler::SendHandler(
	  Handler::PrivateListener* privateListener,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
	  const PeerConnection::Options& peerConnectionOptions,
	  const json& sendingRtpParametersByKind,
	  const json& sendingRemoteRtpParametersByKind)
	  : Handler(
	      privateListener, iceParameters, iceCandidates, dtlsParameters, sctpParameters, peerConnectionOptions)
	{
		MSC_TRACE();

		this->sendingRtpParametersByKind = sendingRtpParametersByKind;

		this->sendingRemoteRtpParametersByKind = sendingRemoteRtpParametersByKind;
	};

	namespace {
		void Normalize(std::vector<webrtc::RtpEncodingParameters>* encodings) {
			if (encodings && encodings->size() > 1)
			{
				uint8_t idx = 0;
				for (webrtc::RtpEncodingParameters& encoding : *encodings)
				{
					encoding.rid = std::string("r").append(std::to_string(idx++));
				}
			}
		}
	}

	void SendHandler::GetDtlsParameters(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track, std::vector<webrtc::RtpEncodingParameters>* encodings, DtlsParametersCallback callback) {
		Normalize(encodings);

		webrtc::RtpTransceiverInit transceiverInit;
		transceiverInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;

		if (encodings && !encodings->empty())
			transceiverInit.send_encodings = *encodings;

		assert(!this->initialTransceiver);
		this->initialTransceiver = this->pc->AddTransceiver(track, transceiverInit);

		Handler::GetDtlsParameters(callback);
	}

	SendHandler::SendResult SendHandler::Send(
	  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
	  std::vector<webrtc::RtpEncodingParameters>* encodings,
	  const json* codecOptions,
	  const json* codec)
	{
		MSC_TRACE();

		// Check if the track is a null pointer.
		if (!track)
			MSC_THROW_TYPE_ERROR("missing track");

		MSC_DEBUG("[kind:%s, track->id():%s]", track->kind().c_str(), track->id().c_str());

		Normalize(encodings);

		json sendingRtpParameters = this->sendingRtpParametersByKind[track->kind()];

		// This may throw.
		sendingRtpParameters["codecs"] = ortc::reduceCodecs(sendingRtpParameters["codecs"], codec);

		json sendingRemoteRtpParameters = this->sendingRemoteRtpParametersByKind[track->kind()];

		// This may throw.
		sendingRemoteRtpParameters["codecs"] =
		  ortc::reduceCodecs(sendingRemoteRtpParameters["codecs"], codec);

		const Sdp::RemoteSdp::MediaSectionIdx mediaSectionIdx = this->remoteSdp->GetNextMediaSectionIdx();

		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver;

		if (this->initialTransceiver) {
			auto initialTrack = this->initialTransceiver->sender()->track();

			if (initialTrack != track) {
				MSC_THROW_INVALID_STATE_ERROR("expected same track used in GetDtlsParameters");
			}

			transceiver = this->initialTransceiver;
			this->initialTransceiver = nullptr;
		} else {
			webrtc::RtpTransceiverInit transceiverInit;
			transceiverInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;

			if (encodings && !encodings->empty())
				transceiverInit.send_encodings = *encodings;

			transceiver = this->pc->AddTransceiver(track, transceiverInit);
		}

		if (!transceiver)
			MSC_THROW_ERROR("error creating transceiver");

		std::string offer;
		std::string localId;

		// Special case for VP9 with SVC.
		bool hackVp9Svc = false;

		try
		{
			offer               = CreateOffer(*pc);
			auto localSdpObject = sdptransform::parse(offer);

			// Transport is not ready.
			if (!this->transportReady)
				this->SetupTransport(localSdpObject);

			std::string scalability_mode =
			  encodings && encodings->size()
			    ? ((*encodings)[0].scalability_mode.has_value() ? (*encodings)[0].scalability_mode.value()
			                                                    : "")
			    : "";

			const json& layers = parseScalabilityMode(scalability_mode);

			auto spatialLayers = layers["spatialLayers"].get<int>();

			auto mimeType = sendingRtpParameters["codecs"][0]["mimeType"].get<std::string>();

			std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

			if (encodings && encodings->size() == 1 && spatialLayers > 1 && mimeType == "video/vp9")
			{
				MSC_DEBUG("send() | enabling legacy simulcast for VP9 SVC");

				hackVp9Svc             = true;
				localSdpObject         = sdptransform::parse(offer);
				json& offerMediaObject = localSdpObject["media"][mediaSectionIdx.idx];

				Sdp::Utils::addLegacySimulcast(offerMediaObject, spatialLayers);

				offer = sdptransform::write(localSdpObject);
			}

			MSC_DEBUG("calling pc->SetLocalDescription():\n%s", offer.c_str());

			SetLocalDescription(*pc, PeerConnection::SdpType::OFFER, offer);

			// We can now get the transceiver.mid.
			localId = transceiver->mid().value();

			// Set MID.
			sendingRtpParameters["mid"] = localId;
		}
		catch (std::exception& error)
		{
			// Panic here. Try to undo things.
			transceiver->SetDirectionWithError(webrtc::RtpTransceiverDirection::kInactive);
			transceiver->sender()->SetTrack(nullptr);

			throw;
		}

		auto localSdp       = this->pc->GetLocalDescription();
		auto localSdpObject = sdptransform::parse(localSdp);

		json& offerMediaObject = localSdpObject["media"][mediaSectionIdx.idx];

		// Set RTCP CNAME.
		sendingRtpParameters["rtcp"]["cname"] = Sdp::Utils::getCname(offerMediaObject);

		// Set RTP encodings by parsing the SDP offer if no encodings are given.
		if (encodings == nullptr || encodings->empty())
		{
			sendingRtpParameters["encodings"] = Sdp::Utils::getRtpEncodings(offerMediaObject);
		}
		// Set RTP encodings by parsing the SDP offer and complete them with given
		// one if just a single encoding has been given.
		else if (encodings->size() == 1)
		{
			auto newEncodings = Sdp::Utils::getRtpEncodings(offerMediaObject);

			fillJsonRtpEncodingParameters(newEncodings.front(), encodings->front());

			// Hack for VP9 SVC.
			if (hackVp9Svc)
				newEncodings = json::array({ newEncodings[0] });

			sendingRtpParameters["encodings"] = newEncodings;
		}
		// Otherwise if more than 1 encoding are given use them verbatim.
		else
		{
			sendingRtpParameters["encodings"] = json::array();

			for (const auto& encoding : *encodings)
			{
				json jsonEncoding = {};

				fillJsonRtpEncodingParameters(jsonEncoding, encoding);
				sendingRtpParameters["encodings"].push_back(jsonEncoding);
			}
		}

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
		  mediaSectionIdx.reuseMid,
		  sendingRtpParameters,
		  sendingRemoteRtpParameters,
		  codecOptions);

		auto answer = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->SetRemoteDescription():\n%s", answer.c_str());

		SetRemoteDescription(*pc, PeerConnection::SdpType::ANSWER, answer);

		// Store in the map.
		this->mapMidTransceiver[localId] = transceiver;

		SendResult sendResult;

		sendResult.localId       = localId;
		sendResult.rtpSender     = transceiver->sender();
		sendResult.rtpParameters = sendingRtpParameters;

		return sendResult;
	}

	Handler::DataChannel SendHandler::SendDataChannel(
	  const std::string& label, webrtc::DataChannelInit dataChannelInit)
	{
		MSC_TRACE();

		uint16_t streamId = this->nextSendSctpStreamId;

		dataChannelInit.negotiated = true;
		dataChannelInit.id         = streamId;

		/* clang-format off */
		json sctpStreamParameters =
		{
			{ "streamId", streamId                  },
			{ "ordered",  dataChannelInit.ordered   },
			{ "protocol", dataChannelInit.protocol  }
		};
		/* clang-format on */

		if (dataChannelInit.maxRetransmitTime.has_value())
		{
			sctpStreamParameters["maxPacketLifeTime"] = dataChannelInit.maxRetransmitTime.value();
		}

		if (dataChannelInit.maxRetransmits.has_value())
		{
			sctpStreamParameters["maxRetransmits"] = dataChannelInit.maxRetransmits.value();
		}

		// This will fill sctpStreamParameters's missing fields with default values.
		ortc::validateSctpStreamParameters(sctpStreamParameters);

		rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel =
		  this->pc->CreateDataChannel(label, &dataChannelInit);

		// Increase next id.
		this->nextSendSctpStreamId = (this->nextSendSctpStreamId + 1) % SctpNumStreamsMis;

		// If this is the first DataChannel we need to create the SDP answer with
		// m=application section.
		if (!this->hasDataChannelMediaSection)
		{
			std::string offer   = CreateOffer(*pc);
			auto localSdpObject = sdptransform::parse(offer);
			const Sdp::RemoteSdp::MediaSectionIdx mediaSectionIdx =
			  this->remoteSdp->GetNextMediaSectionIdx();

			auto offerMediaObject =
			  find_if(localSdpObject["media"].begin(), localSdpObject["media"].end(), [](const json& m) {
				  return m.at("type").get<std::string>() == "application";
			  });

			if (offerMediaObject == localSdpObject["media"].end())
			{
				MSC_THROW_ERROR("Missing 'application' media section in SDP offer");
			}

			if (!this->transportReady)
			{
				this->SetupTransport(localSdpObject);
			}

			MSC_DEBUG("calling pc.setLocalDescription() [offer:%s]", offer.c_str());

			SetLocalDescription(*pc, PeerConnection::SdpType::OFFER, offer);
			this->remoteSdp->SendSctpAssociation(*offerMediaObject);

			auto sdpAnswer = this->remoteSdp->GetSdp();

			MSC_DEBUG("calling pc.setRemoteDescription() [answer:%s]", sdpAnswer.c_str());

			SetRemoteDescription(*pc, PeerConnection::SdpType::ANSWER, sdpAnswer);
			this->hasDataChannelMediaSection = true;
		}

		SendHandler::DataChannel dataChannel;

		dataChannel.dataChannel          = webrtcDataChannel;
		dataChannel.sctpStreamParameters = sctpStreamParameters;

		return dataChannel;
	}

	void SendHandler::StopSending(const std::string& localId)
	{
		MSC_TRACE();

		MSC_DEBUG("[localId:%s]", localId.c_str());

		auto locaIdIt = this->mapMidTransceiver.find(localId);

		if (locaIdIt == this->mapMidTransceiver.end())
			MSC_THROW_ERROR("associated RtpTransceiver not found");

		auto transceiver = locaIdIt->second;

		transceiver->sender()->SetTrack(nullptr);
		this->pc->RemoveTrack(transceiver->sender());
		this->remoteSdp->CloseMediaSection(transceiver->mid().value());

		// May throw.
		auto offer = CreateOffer(*pc);

		MSC_DEBUG("calling pc->SetLocalDescription():\n%s", offer.c_str());

		// May throw.
		SetLocalDescription(*pc, PeerConnection::SdpType::OFFER, offer);

		auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
		auto answer      = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->SetRemoteDescription():\n%s", answer.c_str());

		// May throw.
		SetRemoteDescription(*pc, PeerConnection::SdpType::ANSWER, answer);
	}

	void SendHandler::ReplaceTrack(const std::string& localId, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track)
	{
		MSC_TRACE();

		MSC_DEBUG(
		  "[localId:%s, track->id():%s]",
		  localId.c_str(),
		  track == nullptr ? "nullptr" : track->id().c_str());

		auto localIdIt = this->mapMidTransceiver.find(localId);

		if (localIdIt == this->mapMidTransceiver.end())
			MSC_THROW_ERROR("associated RtpTransceiver not found");

		auto transceiver = localIdIt->second;

		transceiver->sender()->SetTrack(track.get());
	}

	void SendHandler::SetMaxSpatialLayer(const std::string& localId, uint8_t spatialLayer)
	{
		MSC_TRACE();

		MSC_DEBUG("[localId:%s, spatialLayer:%" PRIu8 "]", localId.c_str(), spatialLayer);

		auto localIdIt = this->mapMidTransceiver.find(localId);

		if (localIdIt == this->mapMidTransceiver.end())
			MSC_THROW_ERROR("associated RtpTransceiver not found");

		auto transceiver = localIdIt->second;
		auto parameters   = transceiver->sender()->GetParameters();

		bool hasLowEncoding{ false };
		bool hasMediumEncoding{ false };
		bool hasHighEncoding{ false };
		webrtc::RtpEncodingParameters* lowEncoding{ nullptr };
		webrtc::RtpEncodingParameters* mediumEncoding{ nullptr };
		webrtc::RtpEncodingParameters* highEncoding{ nullptr };

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
			MSC_THROW_ERROR("%s", result.message());
	}

	void SendHandler::GetSenderStats(const std::string& localId, PeerConnection::StatsHandler callback)
	{
		MSC_TRACE();

		MSC_DEBUG("[localId:%s]", localId.c_str());

		rtc::scoped_refptr<webrtc::RtpSenderInterface> sender;

		if (auto itr = this->mapMidTransceiver.find(localId); itr != this->mapMidTransceiver.end()) {
			sender = itr->second->sender();
		}

		pc->GetStats(sender, callback);
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
		auto offer = CreateOffer(*pc, options);

		MSC_DEBUG("calling pc->SetLocalDescription():\n%s", offer.c_str());

		// May throw.
		SetLocalDescription(*pc, PeerConnection::SdpType::OFFER, offer);

		auto localSdpObj = sdptransform::parse(this->pc->GetLocalDescription());
		auto answer      = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->SetRemoteDescription():\n%s", answer.c_str());

		// May throw.
		SetRemoteDescription(*pc, PeerConnection::SdpType::ANSWER, answer);
	}

	/* RecvHandler methods */

	RecvHandler::RecvHandler(
	  Handler::PrivateListener* privateListener,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
	  const PeerConnection::Options& peerConnectionOptions)
	  : Handler(
	      privateListener, iceParameters, iceCandidates, dtlsParameters, sctpParameters, peerConnectionOptions)
	{
		MSC_TRACE();
	};

	void RecvHandler::GetDtlsParameters(const std::string& id, const std::string& kind, const nlohmann::json* rtpParameters, DtlsParametersCallback callback) {
		auto offer = RemoteOffer(id, kind, *rtpParameters);

		assert(!this->initialOffer.has_value());
		this->initialOffer = offer;

		this->pc->SetRemoteDescription(PeerConnection::SdpType::OFFER, offer.sdp, [=] (auto, auto err) {
			if (err.ok()) {
				Handler::GetDtlsParameters(callback);
			} else {
				callback(nullptr, err);
			}
		});
	}

	struct RecvHandler::RemoteOffer RecvHandler::RemoteOffer(const std::string& id, const std::string& kind, const nlohmann::json& rtpParameters) {
		std::string localId;

		// mid is optional, check whether it exists and is a non empty string.
		auto midIt = rtpParameters.find("mid");
		if (midIt != rtpParameters.end() && (midIt->is_string() && !midIt->get<std::string>().empty()))
			localId = midIt->get<std::string>();
		else
			localId = std::to_string(this->mapMidTransceiver.size());

		const auto& cname = rtpParameters["rtcp"]["cname"];

		this->remoteSdp->Receive(localId, kind, rtpParameters, cname, id);

		ConsumerRef consumer(id, kind, rtpParameters);

		return { consumer, localId, this->remoteSdp->GetSdp() };
	}

	RecvHandler::RecvResult RecvHandler::Receive(
	  const std::string& id, const std::string& kind, const json* rtpParameters)
	{
		MSC_TRACE();

		MSC_DEBUG("[id:%s, kind:%s]", id.c_str(), kind.c_str());

		struct RemoteOffer offer;

		if (this->initialOffer.has_value()) {
			offer = this->initialOffer.value();

			if (offer.consumer != ConsumerRef(id, kind, *rtpParameters)) {
				MSC_THROW_INVALID_STATE_ERROR("expected same consumer arguments used in GetDtlsParameters");
			}

			this->initialOffer = {};
		} else {
			offer = RemoteOffer(id, kind, *rtpParameters);

			MSC_DEBUG("calling pc->setRemoteDescription():\n%s", offer.sdp.c_str());

			// May throw.
			SetRemoteDescription(*pc, PeerConnection::SdpType::OFFER, offer.sdp);
		}

		// May throw.
		auto answer         = CreateAnswer(*pc);
		auto localSdpObject = sdptransform::parse(answer);
		auto mediaIt        = find_if(
      localSdpObject["media"].begin(), localSdpObject["media"].end(), [&offer](const json& m) {
        return m["mid"].get<std::string>() == offer.localId;
      });

		auto& answerMediaObject = *mediaIt;

		// May need to modify codec parameters in the answer based on codec
		// parameters in the offer.
		Sdp::Utils::applyCodecParameters(*rtpParameters, answerMediaObject);

		answer = sdptransform::write(localSdpObject);

		if (!this->transportReady)
			this->SetupTransport(localSdpObject);

		MSC_DEBUG("calling pc->SetLocalDescription():\n%s", answer.c_str());

		// May throw.
		SetLocalDescription(*pc, PeerConnection::SdpType::ANSWER, answer);

		auto transceivers  = this->pc->GetTransceivers();
		auto transceiverIt = std::find_if(
		  transceivers.begin(), transceivers.end(), [&offer](auto t) {
			  return t->mid() == offer.localId;
		  });

		if (transceiverIt == transceivers.end())
			MSC_THROW_ERROR("new RTCRtpTransceiver not found");

		auto& transceiver = *transceiverIt;

		// Store in the map.
		this->mapMidTransceiver[offer.localId] = transceiver;

		RecvResult recvResult;

		recvResult.localId     = offer.localId;
		recvResult.rtpReceiver = transceiver->receiver();
		recvResult.track       = transceiver->receiver()->track();

		return recvResult;
	}

	Handler::DataChannel RecvHandler::ReceiveDataChannel(
	  const std::string& label, webrtc::DataChannelInit dataChannelInit)
	{
		MSC_TRACE();

		dataChannelInit.negotiated = true;

		/* clang-format off */
		nlohmann::json sctpStreamParameters =
		{
			{ "streamId", dataChannelInit.id },
			{ "ordered",  dataChannelInit.ordered }
		};
		/* clang-format on */

		// This will fill sctpStreamParameters's missing fields with default values.
		ortc::validateSctpStreamParameters(sctpStreamParameters);

		rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel =
		  this->pc->CreateDataChannel(label, &dataChannelInit);

		// If this is the first DataChannel we need to create the SDP answer with
		// m=application section.
		if (!this->hasDataChannelMediaSection)
		{
			this->remoteSdp->RecvSctpAssociation();
			auto sdpOffer = this->remoteSdp->GetSdp();

			MSC_DEBUG("calling pc->setRemoteDescription() [offer:%s]", sdpOffer.c_str());

			// May throw.
			SetRemoteDescription(*pc, PeerConnection::SdpType::OFFER, sdpOffer);

			auto sdpAnswer = CreateAnswer(*pc);

			if (!this->transportReady)
			{
				auto localSdpObject = sdptransform::parse(sdpAnswer);
				this->SetupTransport(localSdpObject);
			}

			MSC_DEBUG("calling pc->setLocalDescription() [answer: %s]", sdpAnswer.c_str());

			// May throw.
			SetLocalDescription(*pc, PeerConnection::SdpType::ANSWER, sdpAnswer);

			this->hasDataChannelMediaSection = true;
		}

		RecvHandler::DataChannel dataChannel;

		dataChannel.dataChannel          = webrtcDataChannel;
		dataChannel.sctpStreamParameters = sctpStreamParameters;

		return dataChannel;
	}

	void RecvHandler::StopReceiving(const std::string& localId)
	{
		MSC_TRACE();

		MSC_DEBUG("[localId:%s]", localId.c_str());

		auto localIdIt = this->mapMidTransceiver.find(localId);

		if (localIdIt == this->mapMidTransceiver.end())
			MSC_THROW_ERROR("associated RtpTransceiver not found");

		auto& transceiver = localIdIt->second;

		MSC_DEBUG("disabling mid:%s", transceiver->mid().value().c_str());

		this->remoteSdp->CloseMediaSection(transceiver->mid().value());

		// FIXME: Track isn't removed from the PeerConnection?

		auto offer = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->setRemoteDescription():\n%s", offer.c_str());

		// May throw.
		SetRemoteDescription(*pc, PeerConnection::SdpType::OFFER, offer);

		// May throw.
		auto answer = CreateAnswer(*pc);

		MSC_DEBUG("calling pc->SetLocalDescription():\n%s", answer.c_str());

		// May throw.
		SetLocalDescription(*pc, PeerConnection::SdpType::ANSWER, answer);
	}

 	void RecvHandler::GetReceiverStats(const std::string& localId, PeerConnection::StatsHandler callback)
	{
		MSC_TRACE();

		MSC_DEBUG("[localId:%s]", localId.c_str());

		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver;

		if (auto itr = this->mapMidTransceiver.find(localId); itr != this->mapMidTransceiver.end()) {
			receiver = itr->second->receiver();
		}

		pc->GetStats(receiver, callback);
	}

	void RecvHandler::RestartIce(const json& iceParameters)
	{
		MSC_TRACE();

		// Provide the remote SDP handler with new remote ICE parameters.
		this->remoteSdp->UpdateIceParameters(iceParameters);

		if (!this->transportReady)
			return;

		auto offer = this->remoteSdp->GetSdp();

		MSC_DEBUG("calling pc->setRemoteDescription():\n%s", offer.c_str());

		// May throw.
		SetRemoteDescription(*pc, PeerConnection::SdpType::OFFER, offer);

		// May throw.
		auto answer = CreateAnswer(*pc);

		MSC_DEBUG("calling pc->SetLocalDescription():\n%s", answer.c_str());

		// May throw.
		SetLocalDescription(*pc, PeerConnection::SdpType::ANSWER, answer);
	}
} // namespace mediasoupclient

// Private helpers used in this file.

static void fillJsonRtpEncodingParameters(json& jsonEncoding, const webrtc::RtpEncodingParameters& encoding)
{
	MSC_TRACE();

	jsonEncoding["active"] = encoding.active;

	if (!encoding.rid.empty())
		jsonEncoding["rid"] = encoding.rid;

	if (encoding.max_bitrate_bps)
		jsonEncoding["maxBitrate"] = *encoding.max_bitrate_bps;

	if (encoding.max_framerate)
		jsonEncoding["maxFramerate"] = *encoding.max_framerate;

	if (encoding.scale_resolution_down_by)
		jsonEncoding["scaleResolutionDownBy"] = *encoding.scale_resolution_down_by;

	if (encoding.scalability_mode.has_value())
		jsonEncoding["scalabilityMode"] = *encoding.scalability_mode;

	jsonEncoding["networkPriority"] = encoding.network_priority;
}
