#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include "Exception.hpp"
#include "json.hpp"
#include "api/media_stream_interface.h" // MediaStreamTrackInterface
#include <string>

namespace mediasoupclient
{
// Fast forward declarations.
class Transport;
class SendTransport;

class Producer
{
public:
	class PrivateListener
	{
	public:
		virtual void OnClose(Producer* producer) = 0;
		virtual void OnReplaceTrack(
		  const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack)             = 0;
		virtual void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) = 0;
		virtual nlohmann::json OnGetStats(const Producer* producer)                          = 0;
	};

	/* Public Listener API */
	class Listener
	{
	public:
		virtual void OnTransportClose(Producer* producer) = 0;
	};

public:
	const std::string& GetId() const;
	const std::string& GetLocalId() const;
	std::string GetKind() const;
	webrtc::MediaStreamTrackInterface* GetTrack() const;
	const nlohmann::json& GetRtpParameters() const;
	const uint8_t GetMaxSpatialLayer() const;
	nlohmann::json& GetAppData();
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
	  PrivateListener* privateListener,
	  Listener* listener,
	  const std::string& id,
	  const std::string& localId,
	  webrtc::MediaStreamTrackInterface* track,
	  const nlohmann::json& rtpParameters,
	  const nlohmann::json& appData);

	void TransportClosed();

	/* SendTransport will create instances and call private member TransporClosed */
	friend SendTransport;

private:
	// PrivateListener instance.
	PrivateListener* privateListener;

	// Public Listener instance.
	Listener* listener;

	// Id.
	std::string id;

	// localId.
	std::string localId;

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

inline const std::string& Producer::GetLocalId() const
{
	return this->localId;
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

inline nlohmann::json& Producer::GetAppData()
{
	return this->appData;
}

inline nlohmann::json Producer::GetStats() const
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->privateListener->OnGetStats(this);
}
} // namespace mediasoupclient

#endif
