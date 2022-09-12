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
	void Device::Load(nlohmann::json routerRtpCapabilities, const PeerConnection::Options& opts, LoadCallback callback)
	{
		MSC_TRACE();

		Handler::GetNativeRtpCapabilities(opts, [=](auto nativeRtpCapabilities, auto error) mutable {
			std::exception_ptr exception;

			try {
				if (this->loaded)
					MSC_THROW_INVALID_STATE_ERROR("already loaded");

				if (!error.ok()) {
					throw MediaSoupClientError(error.message());
				}

				MSC_DEBUG("got native RTP capabilities:\n%s", nativeRtpCapabilities.dump(4).c_str());

				// This may throw.
				ortc::validateRtpCapabilities(routerRtpCapabilities);
				ortc::validateRtpCapabilities(nativeRtpCapabilities);

				// Get extended RTP capabilities.
				this->extendedRtpCapabilities =
				ortc::getExtendedRtpCapabilities(nativeRtpCapabilities, routerRtpCapabilities);

				MSC_DEBUG("got extended RTP capabilities:\n%s", this->extendedRtpCapabilities.dump(4).c_str());

				// Check whether we can produce audio/video.
				this->canProduceByKind["audio"] = ortc::canSend("audio", this->extendedRtpCapabilities);
				this->canProduceByKind["video"] = ortc::canSend("video", this->extendedRtpCapabilities);

				// Generate our receiving RTP capabilities for receiving media.
				this->recvRtpCapabilities = ortc::getRecvRtpCapabilities(this->extendedRtpCapabilities);

				MSC_DEBUG("got receiving RTP capabilities:\n%s", this->recvRtpCapabilities.dump(4).c_str());

				// This may throw.
				ortc::validateRtpCapabilities(this->recvRtpCapabilities);

				// Generate our SCTP capabilities.
				this->sctpCapabilities = Handler::GetNativeSctpCapabilities();

				MSC_DEBUG("got receiving SCTP capabilities:\n%s", this->sctpCapabilities.dump(4).c_str());

				// This may throw.
				ortc::validateSctpCapabilities(this->sctpCapabilities);
			} catch(...) {
				exception = std::current_exception();
			}

			if (!exception) {
				this->peerConnectionOptions = opts;
				this->loaded = true;
			}

			callback(exception);
		});
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
		  this->peerConnectionOptions,
		  &this->extendedRtpCapabilities,
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
	  const json& appData) const
	{
		MSC_TRACE();

		return Device::CreateSendTransport(
		  listener, id, iceParameters, iceCandidates, dtlsParameters, nullptr, appData);
	}

	RecvTransport* Device::CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& sctpParameters,
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
		  this->peerConnectionOptions,
		  &this->extendedRtpCapabilities,
		  appData);

		return transport;
	}

	RecvTransport* Device::CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const std::string& id,
	  const json& iceParameters,
	  const json& iceCandidates,
	  const json& dtlsParameters,
	  const json& appData) const
	{
		MSC_TRACE();

		return Device::CreateRecvTransport(
		  listener, id, iceParameters, iceCandidates, dtlsParameters, nullptr, appData);
	}
} // namespace mediasoupclient
