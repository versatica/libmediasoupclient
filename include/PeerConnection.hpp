#ifndef MSC_PEERCONNECTION_HPP
#define MSC_PEERCONNECTION_HPP

#include <api/peer_connection_interface.h> // webrtc::PeerConnectionInterface
#include <future>                          // std::promise, std::future
#include <json.hpp>
#include <memory> // std::unique_ptr

namespace mediasoupclient
{
	class PeerConnection
	{
	public:
		enum class SdpType : uint8_t
		{
			OFFER = 0,
			PRANSWER,
			ANSWER
		};

		static std::map<webrtc::SdpType, const webrtc::SdpType> sdpType2webRtcSdpType;
		static std::map<webrtc::PeerConnectionInterface::IceConnectionState, const std::string>
		  iceConnectionState2String;
		static std::map<webrtc::PeerConnectionInterface::IceGatheringState, const std::string>
		  iceGatheringState2String;
		static std::map<webrtc::PeerConnectionInterface::SignalingState, const std::string> signalingState2String;

	public:
		class PrivateListener : public webrtc::PeerConnectionObserver
		{
			/* Virtual methods inherited from PeerConnectionObserver. */
		public:
			void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) override;
			void OnAddStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
			void OnRemoveStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
			void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) override;
			void OnRenegotiationNeeded() override;
			void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;
			void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) override;
			void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
			void OnIceCandidatesRemoved(const std::vector<webrtc::Candidate>& candidates) override;
			void OnIceConnectionReceivingChange(bool receiving) override;
			void OnAddTrack(
			  webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
			  const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
			void OnTrack(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
			void OnRemoveTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
			void OnInterestingUsage(int usagePattern) override;
		};

		class SetLocalDescriptionObserver : public webrtc::SetLocalDescriptionObserverInterface
		{
		public:
			SetLocalDescriptionObserver()           = default;
			~SetLocalDescriptionObserver() override = default;

			std::future<void> GetFuture();
			void Reject(const std::string& error);

			/* Virtual methods inherited from webrtc::SetLocalDescriptionObserver. */
		public:
			void OnSetLocalDescriptionComplete(webrtc::RTCError error) override;

		private:
			std::promise<void> promise;
		};

		class SetRemoteDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface
		{
		public:
			SetRemoteDescriptionObserver()           = default;
			~SetRemoteDescriptionObserver() override = default;

			std::future<void> GetFuture();
			void Reject(const std::string& error);

			/* Virtual methods inherited from webrtc::SetRemoteDescriptionObserver. */
		public:
			void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

		private:
			std::promise<void> promise;
		};

		class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
		{
		public:
			SetSessionDescriptionObserver()           = default;
			~SetSessionDescriptionObserver() override = default;

			std::future<void> GetFuture();
			void Reject(const std::string& error);

			/* Virtual methods inherited from webrtc::SetSessionDescriptionObserver. */
		public:
			void OnSuccess() override;
			void OnFailure(webrtc::RTCError error) override;

		private:
			std::promise<void> promise;
		};

		class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver
		{
		public:
			CreateSessionDescriptionObserver()           = default;
			~CreateSessionDescriptionObserver() override = default;

			std::future<std::string> GetFuture();
			void Reject(const std::string& error);

			/* Virtual methods inherited from webrtc::CreateSessionDescriptionObserver. */
		public:
			void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
			void OnFailure(webrtc::RTCError error) override;

		private:
			std::promise<std::string> promise;
		};

		class RTCStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback
		{
		public:
			RTCStatsCollectorCallback()           = default;
			~RTCStatsCollectorCallback() override = default;

			std::future<nlohmann::json> GetFuture();

			/* Virtual methods inherited from webrtc::RTCStatsCollectorCallback. */
		public:
			void OnStatsDelivered(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

		private:
			std::promise<nlohmann::json> promise;
		};

	public:
		struct Options
		{
			webrtc::PeerConnectionInterface::RTCConfiguration config;
			webrtc::PeerConnectionFactoryInterface* factory{ nullptr };
		};

	public:
		PeerConnection(PrivateListener* privateListener, const Options* options);
		~PeerConnection() = default;

		void Close();
		webrtc::PeerConnectionInterface::RTCConfiguration GetConfiguration() const;
		bool SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config);
		std::string CreateOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);
		std::string CreateAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);
		void SetLocalDescription(webrtc::SdpType type, const std::string& sdp);
		void SetRemoteDescription(webrtc::SdpType type, const std::string& sdp);
		const std::string GetLocalDescription();
		const std::string GetRemoteDescription();
		std::vector<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const;
		webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(webrtc::MediaType mediaType);
		webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(
		  webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
		  webrtc::RtpTransceiverInit rtpTransceiverInit);
		std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders();
		bool RemoveTrack(webrtc::scoped_refptr<webrtc::RtpSenderInterface> sender);
		nlohmann::json GetStats();
		nlohmann::json GetStats(webrtc::scoped_refptr<webrtc::RtpSenderInterface> selector);
		nlohmann::json GetStats(webrtc::scoped_refptr<webrtc::RtpReceiverInterface> selector);
		webrtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
		  const std::string& label, const webrtc::DataChannelInit* config);

	private:
		// Signaling and worker threads.
		std::unique_ptr<webrtc::Thread> networkThread;
		std::unique_ptr<webrtc::Thread> signalingThread;
		std::unique_ptr<webrtc::Thread> workerThread;

		// PeerConnection factory.
		webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory;

		// PeerConnection instance.
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> pc;
	};
} // namespace mediasoupclient

#endif
