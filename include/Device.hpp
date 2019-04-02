#ifndef MSC_DEVICE_HPP
#define MSC_DEVICE_HPP

#include "Exception.hpp"
#include "Handler.hpp"
#include "Transport.hpp"
#include "json.hpp"
#include <map>
#include <string>

namespace mediasoupclient
{
class Device
{
public:
	Device()  = default;
	~Device() = default;

	bool IsLoaded() const;
	const nlohmann::json& GetRtpCapabilities() const;
	void Load(const nlohmann::json& routerRtpCapabilities);
	bool CanProduce(const std::string& kind);

	SendTransport* CreateSendTransport(
	  SendTransport::Listener* listener,
	  const std::string& id,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions = nullptr,
	  nlohmann::json appData                               = nlohmann::json::object()) const;

	RecvTransport* CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const std::string& id,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions = nullptr,
	  nlohmann::json appData                               = nlohmann::json::object()) const;

private:
	// Loaded flag.
	bool loaded{ false };

	// Extended RTP capabilities.
	nlohmann::json extendedRtpCapabilities;

	// Local RTP capabilities for receiving media.
	nlohmann::json recvRtpCapabilities;

	// Whether we can produce audio/video based on computed extended RTP capabilities.
	/* clang-format off */
	std::map<std::string, bool> canProduceByKind =
	{
		{ "audio", false },
		{ "video", false }
	};
	/* clang-format on */
};

/* Inline methods */

/**
 * Whether the Device is loaded.
 */
inline bool Device::IsLoaded() const
{
	return this->loaded;
}

/**
 * RTP capabilities of the Device for receiving media.
 */
inline const nlohmann::json& Device::GetRtpCapabilities() const
{
	if (!this->loaded)
		throw Exception("Not loaded");

	return this->recvRtpCapabilities;
}

/**
 * Whether we can produce audio/video.
 *
 */
inline bool Device::CanProduce(const std::string& kind)
{
	if (!this->loaded)
		throw Exception("Not loaded");

	if (kind != "audio" && kind != "video")
		throw Exception("Invalid kind");

	return this->canProduceByKind[kind];
}
} // namespace mediasoupclient

#endif
