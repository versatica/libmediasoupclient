#define MSC_CLASS "Transport"
// #define MSC_LOG_DEV

#include "Transport.hpp"
#include "Logger.hpp"
#include "VideoLayers.hpp"
#include "ortc.hpp"
#include <string>
#include <utility>

namespace mediasoupclient
{
/* SendTransport instance methods. */

SendTransport::SendTransport(
  Listener* listener,
  const json& transportRemoteParameters,
  const json& iceServers,
  const std::string& iceTransportPolicy,
  const json& proprietaryConstraints,
  const json& extendedRtpCapabilities,
  std::map<std::string, bool> canProduceByKind,
  json appData)
  : listener(listener),
    Transport(listener, transportRemoteParameters, extendedRtpCapabilities, std::move(appData)),
    canProduceByKind(std::move(canProduceByKind))
{
	MSC_TRACE();

	json rtpParametersByKind = {
		{ "audio", ortc::getSendingRtpParameters("audio", extendedRtpCapabilities) },
		{ "video", ortc::getSendingRtpParameters("video", extendedRtpCapabilities) }
	};

	this->handler.reset(new SendHandler({ this,
	                                      transportRemoteParameters,
	                                      iceServers,
	                                      iceTransportPolicy,
	                                      proprietaryConstraints,
	                                      rtpParametersByKind }));

	Transport::SetHandler(this->handler.get());
}

/*
 * Produce a track
 */
std::future<Producer*> SendTransport::Produce(
  Producer::PublicListener* producerPublicListener,
  webrtc::MediaStreamTrackInterface* track,
  bool simulcast,
  const std::string& maxSpatialLayer,
  json appData)
{
	MSC_TRACE();

	std::promise<Producer*> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (track == nullptr)
		return reject(Exception("Track cannot be null"));
	if (track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded)
		return reject(Exception("Track ended"));
	if (simulcast && track->kind() == "audio")
		return reject(Exception("Cannot set simulcast with audio track"));
	if (simulcast && !isValidSpatialLayer(maxSpatialLayer))
		return reject(Exception("Invalid maxSpatialLayer"));

	json rtpParameters;
	json producerRemoteParameters;

	try
	{
		uint8_t numStreams = 1;

		if (simulcast)
		{
			numStreams = (maxSpatialLayer == "high") ? 3 : (maxSpatialLayer == "medium") ? 2 : 1;
		}

		rtpParameters = this->handler->Send(track, numStreams).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	try
	{
		/* clang-format off */
		json producerLocalParameters =
		{
			{ "kind",          track->kind() },
			{ "rtpParameters", rtpParameters },
			{ "appData",       appData       }
		};
		/* clang-format on */

		producerRemoteParameters = this->listener->OnProduce(producerLocalParameters).get();
	}
	catch (Exception& error)
	{
		this->handler->StopSending(track);

		return reject(error);
	}

	auto* producer = new Producer(
	  this,
	  producerPublicListener,
	  producerRemoteParameters["id"].get<std::string>(),
	  track,
	  producerRemoteParameters,
	  appData);

	this->producers[producer->GetId()] = producer;

	promise.set_value(producer);

	return promise.get_future();
}

/* Handler::Listener methods */
std::future<void> Transport::OnConnect(json& transportLocalParameters)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	if (this->closed)
		return reject(Exception("Invalid state"));

	transportLocalParameters["id"] = this->id;

	this->listener->OnConnect(transportLocalParameters);

	promise.set_value();

	return promise.get_future();
}

/* Producer::Listener methods */
void SendTransport::OnClose(Producer* producer)
{
	MSC_TRACE();

	this->producers.erase(producer->GetId());

	if (this->closed)
		return;

	try
	{
		this->handler->StopSending(producer->GetTrack()).get();
	}
	catch (Exception& error)
	{
	}
}

std::future<void> SendTransport::OnReplaceTrack(
  const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack)
{
	MSC_TRACE();

	return this->handler->ReplaceTrack(producer->GetTrack(), newTrack);
}

std::future<void> SendTransport::OnSetMaxSpatialLayer(
  const Producer* producer, const std::string& maxSpatialLayer)
{
	MSC_TRACE();

	return this->handler->SetMaxSpatialLayer(producer->GetTrack(), maxSpatialLayer);
}

std::future<json> SendTransport::OnGetStats(const Producer* producer)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");

	return this->handler->GetSenderStats(producer->GetTrack());
}

/* RecvTransport instance methods. */

RecvTransport::RecvTransport(
  Listener* listener,
  const json& transportRemoteParameters,
  const json& iceServers,
  const std::string& iceTransportPolicy,
  const json& proprietaryConstraints,
  const json& extendedRtpCapabilities,
  json appData)
  : listener(listener),
    Transport(listener, transportRemoteParameters, extendedRtpCapabilities, std::move(appData))
{
	MSC_TRACE();

	this->handler.reset(new RecvHandler(
	  { this, transportRemoteParameters, iceServers, iceTransportPolicy, proprietaryConstraints }));

	Transport::SetHandler(this->handler.get());
}

/*
 * Consume a remote Producer.
 */
std::future<Consumer*> RecvTransport::Consume(
  Consumer::PublicListener* consumerPublicListener, const json& consumerRemoteParameters, json appData)
{
	MSC_TRACE();

	std::promise<Consumer*> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	// Check if the track is a null pointer.
	if (this->closed)
		return reject(Exception("Invalid state"));
	if (!consumerRemoteParameters.is_object())
		return reject(Exception("Missing consumerRemoteParameters"));
	if (consumerRemoteParameters.find("id") == consumerRemoteParameters.end())
		return reject(Exception("Missing consumerRemoteParameters[\"id\"]"));
	if (consumerRemoteParameters.find("producerId") == consumerRemoteParameters.end())
		return reject(Exception("Missing consumerRemoteParameters[\"producerId\"]"));

	json consumerParameters = consumerRemoteParameters;

	// Ensure the device can consume it.
	auto canConsume =
	  ortc::canReceive(consumerParameters["rtpParameters"], this->extendedRtpCapabilities);

	if (!canConsume)
		return reject(Exception("cannot consume this Producer"));

	webrtc::MediaStreamTrackInterface* track;

	try
	{
		track = this->handler
		          ->Receive(
		            consumerParameters["id"].get<std::string>(),
		            consumerParameters["kind"].get<std::string>(),
		            consumerParameters["rtpParameters"])
		          .get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	auto* consumer = new Consumer(
	  this,
	  consumerPublicListener,
	  consumerParameters["id"].get<std::string>(),
	  consumerParameters["producerId"].get<std::string>(),
	  track,
	  consumerParameters["rtpParameters"],
	  std::move(appData));

	this->consumers[consumer->GetId()] = consumer;

	this->listener->OnStartConsumer(consumer);

	promise.set_value(consumer);

	return promise.get_future();
}

/* Consumer::Listener methods */
void RecvTransport::OnClose(Consumer* consumer)
{
	MSC_TRACE();

	this->consumers.erase(consumer->GetId());

	if (this->closed)
		return;

	try
	{
		this->handler->StopReceiving(consumer->GetId()).get();
	}
	catch (Exception& error)
	{
	}
}

std::future<json> RecvTransport::OnGetStats(const Consumer* consumer)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");

	return this->handler->GetReceiverStats(consumer->GetId());
}
} // namespace mediasoupclient
