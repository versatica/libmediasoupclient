#ifndef MSC_DEVICE_HPP
#define MSC_DEVICE_HPP

#include "Exception.hpp"
#include "Handler.hpp"
#include "Transport.hpp"
#include "json.hpp"
#include <map>
#include <string>

using json = nlohmann::json;

class Device
{
public:
	Device()  = default;
	~Device() = default;

	const std::string& GetHandlerName() const;
	bool Loaded() const;
	const json& GetRtpCapabilities() const;
	void Load(const json& routerRtpCapabilities);
	bool CanProduce(const std::string& kind);

	SendTransport* CreateSendTransport(
	  SendTransport::Listener* listener,
	  const json& transportRemoteParameters,
	  const json& iceServers                = json::array(),
	  const std::string& iceTransportPolicy = "",
	  const json& proprietaryConstraints    = json::object(),
	  json appData                          = json::object()) const;

	RecvTransport* CreateRecvTransport(
	  RecvTransport::Listener* listener,
	  const json& transportRemoteParameters,
	  const json& iceServers                = json::array(),
	  const std::string& iceTransportPolicy = "",
	  const json& proprietaryConstraints    = json::object(),
	  json appData                          = json::object()) const;

private:
	// Loaded flag.
	bool loaded{ false };

	// Extended RTP capabilities.
	json extendedRtpCapabilities;

	// Local RTP capabilities for receiving media.
	json recvRtpCapabilities;

	// Whether we can produce audio/video based on computed extended RTP
	// capabilities.
	std::map<std::string, bool> canProduceByKind = { { "audio", false }, { "video", false } };
};

/* Inline methods */

/**
 * The RTC handler class name.
 */
inline const std::string& Device::GetHandlerName() const
{
	return Handler::GetName();
}

/**
 * Whether the Device is loaded.
 */
inline bool Device::Loaded() const
{
	return this->loaded;
}

/**
 * RTP capabilities of the Device for receiving media.
 */
inline const json& Device::GetRtpCapabilities() const
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

#endif
