#define MSC_CLASS "Device"
// #define MSC_LOG_DEV

#include "Device.hpp"

#include "Logger.hpp"
#include "ortc.hpp"
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
/**
 * Initialize the Device.
 */
void Device::Load(const json& routerRtpCapabilities)
{
	MSC_TRACE();

	if (this->loaded)
		throw Exception("Already loaded");
	else if (!routerRtpCapabilities.is_object())
		throw Exception("Missing routerRtpCapabilities");

	// Get Native RTP capabilities.
	auto nativeRtpCapabilities = Handler::GetNativeRtpCapabilities();

	// Get extended RTP capabilities.
	this->extendedRtpCapabilities =
	  ortc::getExtendedRtpCapabilities(nativeRtpCapabilities, routerRtpCapabilities);

	// Check whether we can produce audio/video.
	this->canProduceByKind["audio"] = ortc::canSend("audio", this->extendedRtpCapabilities);
	this->canProduceByKind["video"] = ortc::canSend("video", this->extendedRtpCapabilities);

	// Generate our receiving RTP capabilities for receiving media.
	this->recvRtpCapabilities = ortc::getRecvRtpCapabilities(this->extendedRtpCapabilities);

	MSC_DEBUG("succeeded");

	this->loaded = true;
}

SendTransport* Device::CreateSendTransport(
  SendTransport::Listener* listener,
  const std::string& id,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  const PeerConnection::Options* peerConnectionOptions,
  json appData) const
{
	MSC_TRACE();

	if (!this->loaded)
		throw Exception("not loaded");

	// App data must be a json object.
	if (!appData.is_object())
		throw Exception("appData must be a JSON object");

	// Create a new Transport.
	auto* transport = new SendTransport(
	  listener,
	  id,
	  iceParameters,
	  iceCandidates,
	  dtlsParameters,
	  peerConnectionOptions,
	  &this->extendedRtpCapabilities,
	  &this->canProduceByKind,
	  appData);

	return transport;
}

RecvTransport* Device::CreateRecvTransport(
  Transport::Listener* listener,
  const std::string& id,
  const json& iceParameters,
  const json& iceCandidates,
  const json& dtlsParameters,
  const PeerConnection::Options* peerConnectionOptions,
  json appData) const
{
	MSC_TRACE();

	if (!this->loaded)
		throw Exception("not loaded");

	// App data must be a json object.
	if (!appData.is_object())
		throw Exception("appData must be a JSON object");

	// Create a new Transport.
	auto* transport = new RecvTransport(
	  listener,
	  id,
	  iceParameters,
	  iceCandidates,
	  dtlsParameters,
	  peerConnectionOptions,
	  &this->extendedRtpCapabilities,
	  appData);

	return transport;
}
} // namespace mediasoupclient
