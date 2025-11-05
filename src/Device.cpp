#include "Transport.hpp"
#define MSC_CLASS "Device"

#include "Device.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include "ortc.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	/**
	 * Whether the Device is loaded.
	 */
	bool Device::IsLoaded() const
	{
		MSC_TRACE();

		return this->loaded;
	}

	/**
	 * RTP capabilities of the Device for receiving media.
	 */
	const json& Device::GetRtpCapabilities() const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");

		return this->recvRtpCapabilities;
	}

	/**
	 * SCTP capabilities of the Device for receiving media.
	 */
	const json& Device::GetSctpCapabilities() const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");

		return this->sctpCapabilities;
	}

	/**
	 * Initialize the Device.
	 */
	void Device::Load(json routerRtpCapabilities, const PeerConnection::Options* peerConnectionOptions)
	{
		MSC_TRACE();

		if (this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("already loaded");

		// This may throw.
		ortc::validateRtpCapabilities(routerRtpCapabilities);

		// Get Native RTP capabilities.
		auto nativeRtpCapabilities = Handler::GetNativeRtpCapabilities(peerConnectionOptions);

		MSC_DEBUG("got native RTP capabilities:\n%s", nativeRtpCapabilities.dump(4).c_str());

		// This may throw.
		ortc::validateRtpCapabilities(nativeRtpCapabilities);

		// Get extended RTP capabilities as a function, that SendHandler can invoke to compute the matching capabilities from the current local capabilities.
		// This is required for WebRTC M140+ where header extension ids may differ from the previosly pre-computed sendExtendedRtpCapabilities, resulting in
		// failure when setting the generated SDP answer as RemoteDescription, since the header extension ids in the answer do not match those in the offer.
		// See: https://github.com/versatica/mediasoup-client/pull/336 for the JS counterpart and more rationale.
		this->getSendExtendedRtpCapabilities = [routerRtpCapabilities](json& currentLocalRtpCapabilities) {
			auto routerRtpCapabilitiesCopy = routerRtpCapabilities;
			return ortc::getExtendedRtpCapabilities(currentLocalRtpCapabilities, routerRtpCapabilitiesCopy);
		};
		const auto recvExtendedRtpCapabilities =
			ortc::getExtendedRtpCapabilities(nativeRtpCapabilities, routerRtpCapabilities);

		// Check whether we can produce audio/video.
		this->canProduceByKind["audio"] = ortc::canSend("audio", recvExtendedRtpCapabilities);
		this->canProduceByKind["video"] = ortc::canSend("video", recvExtendedRtpCapabilities);

		// Generate our receiving RTP capabilities for receiving media.
		this->recvRtpCapabilities = ortc::getRecvRtpCapabilities(recvExtendedRtpCapabilities);

		// This may throw.
		ortc::validateRtpCapabilities(this->recvRtpCapabilities);

		MSC_DEBUG("got receiving RTP capabilities:\n%s", this->recvRtpCapabilities.dump(4).c_str());
	
		// Generate our SCTP capabilities.
		this->sctpCapabilities = Handler::GetNativeSctpCapabilities();
		
		// This may throw.
		ortc::validateSctpCapabilities(this->sctpCapabilities);

		MSC_DEBUG("got receiving SCTP capabilities:\n%s", this->sctpCapabilities.dump(4).c_str());

		
		MSC_DEBUG("succeeded");

		this->loaded = true;
	}

	/**
	 * Whether we can produce audio/video.
	 *
	 */
	bool Device::CanProduce(const std::string& kind)
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");
		else if (kind != "audio" && kind != "video")
			MSC_THROW_TYPE_ERROR("invalid kind");

		return this->canProduceByKind[kind];
	}

	SendTransport* Device::CreateSendTransport(
	  SendTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const json& appData) const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");
		else if (!appData.is_object())
			MSC_THROW_TYPE_ERROR("appData must be a JSON object");

		// Validate arguments.
		ortc::validateIceParameters(const_cast<json&>(iceParameters));
		ortc::validateIceCandidates(const_cast<json&>(iceCandidates));
		ortc::validateDtlsParameters(const_cast<json&>(dtlsParameters));

		if (!sctpParameters.is_null())
			ortc::validateSctpParameters(const_cast<json&>(sctpParameters));

		// Create a new Transport.
		auto* transport = new SendTransport(
		  listener,
		  id,
		  iceParameters,
		  iceCandidates,
		  dtlsParameters,
		  sctpParameters,
		  peerConnectionOptions,
		  this->getSendExtendedRtpCapabilities,
		  &this->canProduceByKind,
		  appData);

		return transport;
	}

	SendTransport* Device::CreateSendTransport(
	  SendTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const json& appData) const
	{
		MSC_TRACE();

		return Device::CreateSendTransport(
		  listener, id, iceParameters, iceCandidates, dtlsParameters, nullptr, peerConnectionOptions, appData);
	}

	RecvTransport* Device::CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const json& appData) const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");
		else if (!appData.is_object())
			MSC_THROW_TYPE_ERROR("appData must be a JSON object");

		// Validate arguments.
		ortc::validateIceParameters(const_cast<json&>(iceParameters));
		ortc::validateIceCandidates(const_cast<json&>(iceCandidates));
		ortc::validateDtlsParameters(const_cast<json&>(dtlsParameters));

		if (!sctpParameters.is_null())
			ortc::validateSctpParameters(const_cast<json&>(sctpParameters));

		// Create a new Transport.
		auto* transport = new RecvTransport(
		  listener,
		  id,
		  iceParameters,
		  iceCandidates,
		  dtlsParameters,
		  sctpParameters,
		  peerConnectionOptions,
		  &this->recvRtpCapabilities,
		  appData);

		return transport;
	}

	RecvTransport* Device::CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const json& appData) const
	{
		MSC_TRACE();

		return Device::CreateRecvTransport(
		  listener, id, iceParameters, iceCandidates, dtlsParameters, nullptr, peerConnectionOptions, appData);
	}
} // namespace mediasoupclient
