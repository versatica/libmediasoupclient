#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include "Exception.hpp"
#include "json.hpp"
#include "api/mediastreaminterface.h" // MediaStreamTrackInterface
#include <string>

using json = nlohmann::json;

namespace mediasoupclient
{
// Fast forward declarations.
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
		virtual json OnGetStats(const Producer* producer)                                    = 0;
	};

	/* Public Listener API */
	class PublicListener
	{
	public:
		virtual void OnTransportClose() = 0;
	};

public:
	const std::string& GetId() const;
	std::string GetKind() const;
	webrtc::MediaStreamTrackInterface* GetTrack() const;
	const json& GetRtpParameters() const;
	const uint8_t GetMaxSpatialLayer() const;
	const json& GetAppData() const;
	json GetStats() const;

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
	  json rtpParameters,
	  json appData = json::object());

	void TransportClosed();

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
	json rtpParameters;

	// Video Max spatial layer.
	uint8_t maxSpatialLayer{ 0 };

	// App custom data.
	json appData;
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

inline const json& Producer::GetRtpParameters() const
{
	return this->rtpParameters;
}

inline const uint8_t Producer::GetMaxSpatialLayer() const
{
	return this->maxSpatialLayer;
}

inline const json& Producer::GetAppData() const
{
	return this->appData;
}

inline json Producer::GetStats() const
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->listener->OnGetStats(this);
}
} // namespace mediasoupclient

#endif
