#define MSC_CLASS "Transport"
// #define MSC_LOG_DEV

#include "Transport.hpp"
#include "Logger.hpp"
#include "ortc.hpp"
#include <string>
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
/* SendTransport static variables. */

/* SendTransport instance methods. */

SendTransport::SendTransport(
  Listener* listener,
  const std::string& id,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  PeerConnection::Options* peerConnectionOptions,
  const json& extendedRtpCapabilities,
  std::map<std::string, bool> canProduceByKind,
  json appData)

  : Transport(listener, id, extendedRtpCapabilities, std::move(appData)), listener(listener),
    canProduceByKind(std::move(canProduceByKind))
{
	MSC_TRACE();

	json sendingRtpParametersByKind = {
		{ "audio", ortc::getSendingRtpParameters("audio", extendedRtpCapabilities) },
		{ "video", ortc::getSendingRtpParameters("video", extendedRtpCapabilities) }
	};

	json sendingRemoteRtpParametersByKind = {
		{ "audio", ortc::getSendingRemoteRtpParameters("audio", extendedRtpCapabilities) },
		{ "video", ortc::getSendingRemoteRtpParameters("video", extendedRtpCapabilities) }
	};

	this->handler.reset(new SendHandler(
	  this,
	  iceParameters,
	  iceCandidates,
	  dtlsParameters,
	  peerConnectionOptions,
	  sendingRtpParametersByKind,
	  sendingRemoteRtpParametersByKind));

	Transport::SetHandler(this->handler.get());
}

/*
 * Produce a track
 */
Producer* SendTransport::Produce(
  Producer::Listener* producerListener, webrtc::MediaStreamTrackInterface* track, json appData)
{
	MSC_TRACE();

	static const std::vector<webrtc::RtpEncodingParameters> Encodings;
	static const json CodecOptions = json::object();

	return this->Produce(producerListener, track, Encodings, CodecOptions, std::move(appData));
}

/*
 * Produce a track
 */
Producer* SendTransport::Produce(
  Producer::Listener* producerListener,
  webrtc::MediaStreamTrackInterface* track,
  const std::vector<webrtc::RtpEncodingParameters>& encodings,
  const json& codecOptions,
  const json appData)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");
	else if (track == nullptr)
		throw Exception("Track cannot be null");
	else if (track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded)
		throw Exception("Track ended");
	else if (this->canProduceByKind.find(track->kind()) == this->canProduceByKind.end())
		throw Exception("Cannot produce track kind");
	else if (!appData.is_object())
		throw Exception("appData must be an object");

	std::string producerId;

	std::vector<webrtc::RtpEncodingParameters> normalizedEncodings;

	std::for_each(
	  encodings.begin(),
	  encodings.end(),
	  [&normalizedEncodings](const webrtc::RtpEncodingParameters& entry) {
		  webrtc::RtpEncodingParameters encoding;

		  encoding.active                   = entry.active;
		  encoding.dtx                      = entry.dtx;
		  encoding.max_bitrate_bps          = entry.max_bitrate_bps;
		  encoding.max_framerate            = entry.max_framerate;
		  encoding.scale_framerate_down_by  = entry.scale_framerate_down_by;
		  encoding.scale_resolution_down_by = entry.scale_resolution_down_by;

		  normalizedEncodings.push_back(encoding);
	  });

	// May throw.
	auto result = this->handler->Send(track, normalizedEncodings, codecOptions);

	auto localId       = result.first;
	auto rtpParameters = result.second;

	try
	{
		// May throw.
		producerId = this->listener->OnProduce(track->kind(), rtpParameters, appData).get();
	}
	catch (Exception& error)
	{
		this->handler->StopSending(localId);

		throw;
	}

	auto* producer =
	  new Producer(this, producerListener, producerId, localId, track, rtpParameters, appData);

	this->producers[producer->GetId()] = producer;

	return producer;
}

/* Handler::Listener methods */
void Transport::OnConnect(json& dtlsParameters)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");

	return this->listener->OnConnect(this, dtlsParameters).get();
}

/* Producer::Listener methods */
void SendTransport::OnClose(Producer* producer)
{
	MSC_TRACE();

	this->producers.erase(producer->GetId());

	if (this->closed)
		return;

	// May throw.
	this->handler->StopSending(producer->GetLocalId());
}

void SendTransport::OnReplaceTrack(const Producer* producer, webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	return this->handler->ReplaceTrack(producer->GetLocalId(), track);
}

void SendTransport::OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer)
{
	MSC_TRACE();

	return this->handler->SetMaxSpatialLayer(producer->GetLocalId(), maxSpatialLayer);
}

json SendTransport::OnGetStats(const Producer* producer)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");

	return this->handler->GetSenderStats(producer->GetLocalId());
}

/* RecvTransport instance methods. */

RecvTransport::RecvTransport(
  Transport::Listener* listener,
  const std::string& id,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  PeerConnection::Options* peerConnectionOptions,
  const json& extendedRtpCapabilities,
  json appData)
  : Transport(listener, id, extendedRtpCapabilities, std::move(appData))
{
	MSC_TRACE();

	this->handler.reset(
	  new RecvHandler(this, iceParameters, iceCandidates, dtlsParameters, peerConnectionOptions));

	Transport::SetHandler(this->handler.get());
}

/*
 * Consume a remote Producer.
 */
Consumer* RecvTransport::Consume(
  Consumer::Listener* consumerListener,
  const std::string& id,
  const std::string& producerId,
  const std::string& kind,
  const json& rtpParameters,
  json appData)
{
	MSC_TRACE();

	// Check if the track is a null pointer.
	if (this->closed)
		throw Exception("Invalid state");

	// Ensure the device can consume it.
	auto canConsume = ortc::canReceive(rtpParameters, this->extendedRtpCapabilities);

	if (!canConsume)
		throw Exception("cannot consume this Producer");

	// May throw.
	auto result = this->handler->Receive(id, kind, rtpParameters);

	auto localId = result.first;
	auto* track  = result.second;

	auto* consumer = new Consumer(
	  this, consumerListener, id, localId, producerId, track, rtpParameters, std::move(appData));

	this->consumers[consumer->GetId()] = consumer;

	return consumer;
}

/* Consumer::Listener methods */
void RecvTransport::OnClose(Consumer* consumer)
{
	MSC_TRACE();

	this->consumers.erase(consumer->GetId());

	if (this->closed)
		return;

	// May throw.
	this->handler->StopReceiving(consumer->GetLocalId());
}

json RecvTransport::OnGetStats(const Consumer* consumer)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");

	return this->handler->GetReceiverStats(consumer->GetLocalId());
}
} // namespace mediasoupclient
