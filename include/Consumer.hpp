#ifndef MSC_CONSUMER_HPP
#define MSC_CONSUMER_HPP

#include "json.hpp"
#include "api/mediastreaminterface.h" // MediaStreamTrackInterface
#include <future>
#include <string>

using json = nlohmann::json;

namespace mediasoupclient
{
class Consumer
{
public:
	class Listener
	{
	public:
		virtual void OnClose(Consumer* consumer)                       = 0;
		virtual std::future<json> OnGetStats(const Consumer* consumer) = 0;
	};

	/* Public Listener API */
	class PublicListener
	{
	public:
		virtual void OnTransportClose() = 0;
	};

public:
	Consumer(
	  Listener* listener,
	  PublicListener* publicListener,
	  std::string id,
	  std::string producerId,
	  webrtc::MediaStreamTrackInterface* track,
	  json rtpParameters,
	  json appData);

	const std::string& GetId() const;
	const std::string& GetProducerId() const;
	const std::string GetKind() const;
	webrtc::MediaStreamTrackInterface* GetTrack() const;
	const json& GetRtpParameters() const;
	const json& GetAppData() const;
	std::future<json> GetStats() const;

	bool IsClosed() const;
	bool IsPaused() const;

	void Close();
	void TransportClosed();
	void Pause();
	void Resume();

private:
	// Listener instance.
	Listener* listener;

	// Public listener instance.
	PublicListener* publicListener;

	// Id.
	std::string id;

	// Producer Id.
	std::string producerId;

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
	json appData{};
};

/* Inline methods */

inline const std::string& Consumer::GetId() const
{
	return this->id;
}

inline const std::string& Consumer::GetProducerId() const
{
	return this->producerId;
}

inline const std::string Consumer::GetKind() const
{
	return this->track->kind();
}

inline webrtc::MediaStreamTrackInterface* Consumer::GetTrack() const
{
	return this->track;
}

inline const json& Consumer::GetRtpParameters() const
{
	return this->rtpParameters;
}

inline const json& Consumer::GetAppData() const
{
	return this->appData;
}

inline std::future<json> Consumer::GetStats() const
{
	return this->listener->OnGetStats(this);
}

inline bool Consumer::IsClosed() const
{
	return this->closed;
}

inline bool Consumer::IsPaused() const
{
	return !this->track->enabled();
}
} // namespace mediasoupclient

#endif
