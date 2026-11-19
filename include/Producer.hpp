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
			virtual ~PrivateListener() = default;

			virtual void OnClose(Producer* producer) = 0;

			virtual void OnReplaceTrack(
			  const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack) = 0;

			virtual void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) = 0;

			virtual nlohmann::json OnGetStats(const Producer* producer) = 0;
		};

		/* Public Listener API */
		class Listener
		{
		public:
			virtual ~Listener() = default;

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
		/**
		 * Producer id.
		 */
		const std::string& GetId() const;

		/**
		 * Local id.
		 */
		const std::string& GetLocalId() const;

		/**
		 * Whether the Producer is closed.
		 */
		bool IsClosed() const;

		/**
		 * Media kind.
		 */
		std::string GetKind() const;

		/**
		 * Associated RtpSender.
		 */
		webrtc::RtpSenderInterface* GetRtpSender() const;

		/**
		 * The associated track.
		 */
		webrtc::MediaStreamTrackInterface* GetTrack() const;

		/**
		 * RTP parameters.
		 */
		const nlohmann::json& GetRtpParameters() const;

		/**
		 * Whether the Producer is paused.
		 */
		bool IsPaused() const;

		/**
		 * Max spatial layer being sent.
		 */
		uint8_t GetMaxSpatialLayer() const;

		/**
		 * App custom data.
		 */
		nlohmann::json& GetAppData();

		/**
		 * Closes the Producer.
		 *
		 * @remarks
		 * - The application owns the Producer, so it must destroy it after closing it.
		 */
		void Close();

		/**
		 * Get associated RtpSender stats.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Producer is closed.
		 */
		nlohmann::json GetStats() const;

		/**
		 * Pauses sending media.
		 */
		void Pause();

		/**
		 * Resumes sending media.
		 */
		void Resume();

		/**
		 * Replaces the current track with a new one.
		 *
		 * @param track - Track to send.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Producer is closed or the
		 *   given track is ended.
		 * @throws MediaSoupClientTypeError if no track is given.
		 */
		void ReplaceTrack(webrtc::MediaStreamTrackInterface* track);

		/**
		 * Sets the video max spatial layer to be sent.
		 *
		 * @param spatialLayer - Spatial layer to send.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Producer is closed.
		 * @throws MediaSoupClientTypeError if this is not a video Producer.
		 */
		void SetMaxSpatialLayer(uint8_t spatialLayer);

	private:
		void TransportClosed();

		/* SendTransport will create instances and call private member TransportClosed */
		friend SendTransport;

	private:
		// PrivateListener instance.
		PrivateListener* const privateListener;
		// Public Listener instance.
		Listener* const listener;
		// Id.
		const std::string id;
		// localId.
		const std::string localId;
		// Closed flag.
		bool closed{ false };
		// Associated RTCRtpSender.
		webrtc::RtpSenderInterface* const rtpSender;
		// Local track.
		webrtc::MediaStreamTrackInterface* track;
		// RTP parameters.
		const nlohmann::json rtpParameters;
		// Paused flag.
		bool paused{ false };
		// Video Max spatial layer.
		uint8_t maxSpatialLayer{ 0 };
		// App custom data.
		nlohmann::json appData;
	};
} // namespace mediasoupclient

#endif
