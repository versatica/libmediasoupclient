#ifndef MSC_PEERCONNECTION_HPP
#define MSC_PEERCONNECTION_HPP

#include <json.hpp>
#include <api/peer_connection_interface.h> // webrtc::PeerConnectionInterface
#include <future>                          // std::promise, std::future
#include <memory>                          // std::unique_ptr

#if !defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON) && RTC_DCHECK_IS_ON
#error RTC dchecks are on implicitly; this will corrupt memory if WebRTC is compiled in a release build
#endif

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

		static std::map<PeerConnection::SdpType, const std::string> sdpType2String;
		static std::map<webrtc::PeerConnectionInterface::IceConnectionState, const std::string>
		  iceConnectionState2String;
		static std::map<webrtc::PeerConnectionInterface::IceGatheringState, const std::string>
		  iceGatheringState2String;
		static std::map<webrtc::PeerConnectionInterface::SignalingState, const std::string> signalingState2String;

		static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> DefaultFactory();

	public:
		class PrivateListener : public webrtc::PeerConnectionObserver
		{
			/* Virtual methods inherited from PeerConnectionObserver. */
		public:
			void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) override;
			void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
			void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
			void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) override;
			void OnRenegotiationNeeded() override;
			void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;
			void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) override;
			void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
			void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) override;
			void OnIceConnectionReceivingChange(bool receiving) override;
			void OnAddTrack(
			  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
			  const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
			void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
			void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
			void OnInterestingUsage(int usagePattern) override;
		};

	public:
		struct Options
		{
			webrtc::PeerConnectionInterface::RTCConfiguration config;
			rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory{ nullptr };
		};

	public:
		PeerConnection(PrivateListener* privateListener): PeerConnection(privateListener, Options()) {};
		PeerConnection(PrivateListener* privateListener, const Options& options);

		~PeerConnection() = default;

		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> Factory() const { return factory; }

		void Close();

		webrtc::PeerConnectionInterface::RTCConfiguration GetConfiguration() const;
		bool SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config);

		using SDPHandler = std::function<void(std::string, webrtc::RTCError)>;

		void CreateOffer(SDPHandler fn) { { CreateOffer({}, fn); }}
		void CreateOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&, SDPHandler);

		void CreateAnswer(SDPHandler fn) { CreateAnswer({}, fn); }
		void CreateAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions&, SDPHandler);

		void SetLocalDescription(PeerConnection::SdpType, const std::string&, SDPHandler);
		void SetRemoteDescription(PeerConnection::SdpType, const std::string&, SDPHandler);

		const std::string GetLocalDescription();
		const std::string GetRemoteDescription();

		std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const;
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(cricket::MediaType mediaType);
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(
		  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
		  webrtc::RtpTransceiverInit rtpTransceiverInit);
		std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders();
		bool RemoveTrack(webrtc::RtpSenderInterface* sender);

		using RTCStatsReport = rtc::scoped_refptr<const webrtc::RTCStatsReport>;
		using StatsHandler = std::function<void(const RTCStatsReport &, webrtc::RTCError)>;

		void GetStats(StatsHandler);
		void GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface>, StatsHandler);
		void GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface>, StatsHandler);

		rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
		  const std::string& label, const webrtc::DataChannelInit* config);

	private:
		// PeerConnection instance.
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
	};
} // namespace mediasoupclient

#endif
