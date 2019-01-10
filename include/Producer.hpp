#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include "json.hpp"
#include "webrtc/api/mediastreaminterface.h" // MediaStreamTrackInterface
#include <future>
#include <string>

using json = nlohmann::json;

class Producer
{
public:
	class Listener
	{
	public:
		virtual void OnClose(Producer* producer) = 0;
		virtual std::future<void> OnReplaceTrack(
		  const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack) = 0;
		virtual std::future<void> OnSetMaxSpatialLayer(
		  const Producer* producer, const std::string& maxSpatialLayer) = 0;
		virtual std::future<json> OnGetStats(const Producer* producer)  = 0;
	};

	/* Public Listener API */
	class PublicListener
	{
	public:
		virtual void OnTransportClose() = 0;
	};

public:
	Producer(
	  Listener* listener,
	  PublicListener* publicListener,
	  std::string id,
	  webrtc::MediaStreamTrackInterface* track,
	  json rtpParameters,
	  json appData);

	const std::string& GetId() const;
	webrtc::MediaStreamTrackInterface* GetTrack() const;
	const std::string& GetMaxSpatialLayer() const;
	const json& GetAppData() const;
	std::future<json> GetStats() const;

	bool IsClosed() const;
	bool IsPaused() const;

	void Close();
	void TransportClosed();
	void Pause();
	void Resume();
	std::future<void> ReplaceTrack(webrtc::MediaStreamTrackInterface* track);
	std::future<void> SetMaxSpatialLayer(const std::string& spatialLayer);

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

	// Max spatial layer.
	std::string maxSpatialLayer{};

	// App custom data.
	json appData;
};

/* Inline methods */

inline const std::string& Producer::GetId() const
{
	return this->id;
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

inline const std::string& Producer::GetMaxSpatialLayer() const
{
	return this->maxSpatialLayer;
}

inline const json& Producer::GetAppData() const
{
	return this->appData;
}

inline std::future<json> Producer::GetStats() const
{
	return this->listener->OnGetStats(this);
}

#endif
