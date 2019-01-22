#define MSC_CLASS "Device"
// #define MSC_LOG_DEV

#include "Device.hpp"

#include "Logger.hpp"
#include "ortc.hpp"
#include <utility>

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
	  ortc::getExtendedCapabilities(nativeRtpCapabilities, routerRtpCapabilities);

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
  const json& transportRemoteParameters,
  const json iceServers,
  const std::string& iceTransportPolicy,
  const json proprietaryConstraints,
  json appData) const
{
	MSC_TRACE();

	if (!this->loaded)
		throw Exception("not loaded");
	else if (!transportRemoteParameters.is_object())
		throw Exception("missing transportRemoteParameters");
	else if (transportRemoteParameters.find("id") == transportRemoteParameters.end())
		throw Exception("missing transportRemoteParameters[\"id\"]");

	// Create a new Transport.
	auto* transport = new SendTransport(
	  listener,
	  transportRemoteParameters,
	  iceServers,
	  iceTransportPolicy,
	  proprietaryConstraints,
	  this->extendedRtpCapabilities,
	  this->canProduceByKind,
	  std::move(appData));

	return transport;
}

RecvTransport* Device::CreateRecvTransport(
  Transport::Listener* listener,
  const json& transportRemoteParameters,
  const json& iceServers,
  const std::string& iceTransportPolicy,
  const json& proprietaryConstraints,
  json appData) const
{
	MSC_TRACE();

	if (!this->loaded)
		throw Exception("not loaded");
	else if (!transportRemoteParameters.is_object())
		throw Exception("missing transportRemoteParameters");
	else if (transportRemoteParameters.find("id") == transportRemoteParameters.end())
		throw Exception("missing transportRemoteParameters[\"id\"]");

	// Create a new Transport.
	auto* transport = new RecvTransport(
	  listener,
	  transportRemoteParameters,
	  iceServers,
	  iceTransportPolicy,
	  proprietaryConstraints,
	  this->extendedRtpCapabilities,
	  std::move(appData));

	return transport;
}
} // namespace mediasoupclient
