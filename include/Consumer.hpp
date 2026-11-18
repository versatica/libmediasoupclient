#ifndef MSC_CONSUMER_HPP
#define MSC_CONSUMER_HPP

#include <json.hpp>
#include <api/media_stream_interface.h> // webrtc::MediaStreamTrackInterface
#include <api/rtp_receiver_interface.h> // webrtc::RtpReceiverInterface
#include <string>

namespace mediasoupclient
{
	// Fast forward declarations.
	class Transport;
	class RecvTransport;

	class Consumer
	{
	public:
		class PrivateListener
		{
		public:
			virtual ~PrivateListener() = default;

			virtual void OnClose(Consumer* consumer) = 0;

			virtual nlohmann::json OnGetStats(const Consumer* consumer) = 0;
		};

		/* Public Listener API */
		class Listener
		{
		public:
			virtual ~Listener() = default;

			virtual void OnTransportClose(Consumer* consumer) = 0;
		};

	private:
		Consumer(
		  PrivateListener* privateListener,
		  Listener* listener,
		  const std::string& id,
		  const std::string& localId,
		  const std::string& producerId,
		  webrtc::RtpReceiverInterface* rtpReceiver,
		  webrtc::MediaStreamTrackInterface* track,
		  const nlohmann::json& rtpParameters,
		  const nlohmann::json& appData);

	public:
		/**
		 * Consumer id.
		 */
		const std::string& GetId() const;

		/**
		 * Local id.
		 */
		const std::string& GetLocalId() const;

		/**
		 * Associated Producer id.
		 */
		const std::string& GetProducerId() const;

		/**
		 * Whether the Consumer is closed.
		 */
		bool IsClosed() const;

		/**
		 * Media kind.
		 */
		std::string GetKind() const;

		/**
		 * Associated RtpReceiver.
		 */
		webrtc::RtpReceiverInterface* GetRtpReceiver() const;

		/**
		 * The associated track.
		 */
		webrtc::MediaStreamTrackInterface* GetTrack() const;

		/**
		 * RTP parameters.
		 */
		const nlohmann::json& GetRtpParameters() const;

		/**
		 * Whether the Consumer is paused.
		 */
		bool IsPaused() const;

		/**
		 * App custom data.
		 */
		nlohmann::json& GetAppData();

		/**
		 * Closes the Consumer.
		 *
		 * @remarks
		 * - The application owns the Consumer, so it must destroy it after closing it.
		 */
		void Close();

		/**
		 * Get associated RtpReceiver stats.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Consumer is closed.
		 */
		nlohmann::json GetStats() const;

		/**
		 * Pauses receiving media.
		 */
		void Pause();

		/**
		 * Resumes receiving media.
		 */
		void Resume();

	private:
		void TransportClosed();

		// RecvTransport will create instances and call private member TransporClosed.
		friend RecvTransport;

	private:
		// PrivateListener instance.
		PrivateListener* const privateListener;
		// Public Listener instance.
		Listener* const listener;
		// Id.
		const std::string id;
		// localId.
		const std::string localId;
		// Producer Id.
		const std::string producerId;
		// Closed flag.
		bool closed{ false };
		// Associated RTCRtpReceiver.
		webrtc::RtpReceiverInterface* const rtpReceiver;
		// Local track.
		webrtc::MediaStreamTrackInterface* const track;
		// RTP parameters.
		const nlohmann::json rtpParameters;
		// Paused flag.
		bool paused{ false };
		// App custom data.
		nlohmann::json appData;
	};
} // namespace mediasoupclient

#endif
