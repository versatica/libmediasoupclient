#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include "Exception.hpp"
#include "json.hpp"
#include "api/mediastreaminterface.h" // MediaStreamTrackInterface
#include <string>

namespace mediasoupclient
{
// Fast forward declarations.
class Transport;
class SendTransport;

class Producer
{
public:
	class Listener
	{
	public:
		virtual void OnClose(Producer* producer) = 0;
		virtual void OnReplaceTrack(
		  const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack)             = 0;
		virtual void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) = 0;
		virtual nlohmann::json OnGetStats(const Producer* producer)                          = 0;
	};

	/* Public Listener API */
	class PublicListener
	{
	public:
		virtual void OnTransportClose(Transport* transport) = 0;
	};

public:
	const std::string& GetId() const;
	std::string GetKind() const;
	webrtc::MediaStreamTrackInterface* GetTrack() const;
	const nlohmann::json& GetRtpParameters() const;
	const uint8_t GetMaxSpatialLayer() const;
	const nlohmann::json& GetAppData() const;
	nlohmann::json GetStats() const;

	bool IsClosed() const;
	bool IsPaused() const;

	void Close();
	void Pause();
	void Resume();
	void ReplaceTrack(webrtc::MediaStreamTrackInterface* track);
	void SetMaxSpatialLayer(uint8_t spatialLayer);

private:
	Producer(
	  Listener* listener,
	  PublicListener* publicListener,
	  std::string id,
	  webrtc::MediaStreamTrackInterface* track,
	  nlohmann::json rtpParameters,
	  nlohmann::json appData = nlohmann::json::object());

	void TransportClosed(Transport* transport);

	/* SendTransport will create instances and call private member TransporClosed */
	friend SendTransport;

private:
	// Listener instance.
	Listener* listener;

	// Public listener instance.
	PublicListener* publicListener;

	// Id.
	std::string id;

	// Closed flag.
	bool closed{ false };

	// Paused flag.
	bool paused{ false };

	// Local track.
	webrtc::MediaStreamTrackInterface* track;

	// RTP parameters.
	nlohmann::json rtpParameters;

	// Video Max spatial layer.
	uint8_t maxSpatialLayer{ 0 };

	// App custom data.
	nlohmann::json appData;
};

/* Inline methods */

inline const std::string& Producer::GetId() const
{
	return this->id;
}

inline std::string Producer::GetKind() const
{
	return this->track->kind();
}
inline bool Producer::IsClosed() const
{
	return this->closed;
}

inline bool Producer::IsPaused() const
{
	return !this->track->enabled();
}

inline webrtc::MediaStreamTrackInterface* Producer::GetTrack() const
{
	return this->track;
}

inline const nlohmann::json& Producer::GetRtpParameters() const
{
	return this->rtpParameters;
}

inline const uint8_t Producer::GetMaxSpatialLayer() const
{
	return this->maxSpatialLayer;
}

inline const nlohmann::json& Producer::GetAppData() const
{
	return this->appData;
}

inline nlohmann::json Producer::GetStats() const
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->listener->OnGetStats(this);
}
} // namespace mediasoupclient

#endif
