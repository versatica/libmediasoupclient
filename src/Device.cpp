#define MSC_CLASS "Device"
// #define MSC_LOG_DEV

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
	 * TODO: Tons of debug logs here are missing.
	 * Initialize the Device.
	 */
	void Device::Load(json routerRtpCapabilities, const PeerConnection::Options* peerConnectionOptions)
	{
		MSC_TRACE();

		if (this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("already loaded");

		// Get Native RTP capabilities.
		auto nativeRtpCapabilities = Handler::GetNativeRtpCapabilities(peerConnectionOptions);

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
	  const PeerConnection::Options* peerConnectionOptions,
	  const json& appData) const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");
		else if (!appData.is_object())
			MSC_THROW_TYPE_ERROR("appData must be a JSON object");

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
	  const json& appData) const
	{
		MSC_TRACE();

		if (!this->loaded)
			MSC_THROW_INVALID_STATE_ERROR("not loaded");
		else if (!appData.is_object())
			MSC_THROW_TYPE_ERROR("appData must be a JSON object");

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
