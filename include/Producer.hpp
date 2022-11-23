#ifndef MSC_PRODUCER_HPP
#define MSC_PRODUCER_HPP

#include <json.hpp>
#include <api/media_stream_interface.h> // webrtc::MediaStreamTrackInterface
#include <api/rtp_sender_interface.h>   // webrtc::RtpSenderInterface
#include <string>

#include "PeerConnection.hpp"

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
			  const Producer* producer, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> newTrack) = 0;
			virtual void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) = 0;
			virtual void OnGetStats(const Producer* producer, PeerConnection::StatsHandler)      = 0;
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
		  rtc::scoped_refptr<webrtc::RtpSenderInterface> rtpSender,
		  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
		  const nlohmann::json& rtpParameters,
		  const nlohmann::json& appData);

	public:
		const std::string& GetId() const;
		const std::string& GetLocalId() const;
		bool IsClosed() const;
		std::string GetKind() const;
		rtc::scoped_refptr<webrtc::RtpSenderInterface> GetRtpSender() const;
		rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> GetTrack() const;
		const nlohmann::json& GetRtpParameters() const;
		bool IsPaused() const;
		uint8_t GetMaxSpatialLayer() const;
		nlohmann::json& GetAppData();
		void Close();
		void GetStats(PeerConnection::StatsHandler) const;
		void Pause();
		void Resume();
		void ReplaceTrack(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);
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
		rtc::scoped_refptr<webrtc::RtpSenderInterface> rtpSender;
		// Local track.
		rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
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
