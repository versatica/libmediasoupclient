#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include <json.hpp>
#include <api/media_stream_interface.h> // webrtc::MediaStreamTrackInterface
#include <api/rtp_sender_interface.h>   // webrtc::RtpSenderInterface
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

	private:
		Producer(
		  PrivateListener* privateListener,
		  Listener* listener,
		  const std::string& id,
		  const std::string& localId,
		  webrtc::RtpSenderInterface* rtpSender,
		  webrtc::MediaStreamTrackInterface* track,
		  const nlohmann::json& rtpParameters,
		  const nlohmann::json& appData);

	public:
		const std::string& GetId() const;
		const std::string& GetLocalId() const;
		bool IsClosed() const;
		std::string GetKind() const;
		webrtc::RtpSenderInterface* GetRtpSender() const;
		webrtc::MediaStreamTrackInterface* GetTrack() const;
		const nlohmann::json& GetRtpParameters() const;
		bool IsPaused() const;
		uint8_t GetMaxSpatialLayer() const;
		nlohmann::json& GetAppData();
		void Close();
		nlohmann::json GetStats() const;
		void Pause();
		void Resume();
		void ReplaceTrack(webrtc::MediaStreamTrackInterface* track);
		void SetMaxSpatialLayer(uint8_t spatialLayer);

	private:
		void TransportClosed();

		/* SendTransport will create instances and call private member TransportClosed */
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
		// Associated RTCRtpSender.
		webrtc::RtpSenderInterface* rtpSender{ nullptr };
		// Local track.
		webrtc::MediaStreamTrackInterface* track{ nullptr };
		// RTP parameters.
		nlohmann::json rtpParameters;
		// Paused flag.
		bool paused{ false };
		// Video Max spatial layer.
		uint8_t maxSpatialLayer{ 0 };
		// App custom data.
		nlohmann::json appData;
	};
} // namespace mediasoupclient

#endif
