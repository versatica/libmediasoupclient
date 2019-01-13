#ifndef MSC_PEERCONNECTION_HPP
#define MSC_PEERCONNECTION_HPP

#include "Exception.hpp"
#include "json.hpp"
#include "api/peerconnectioninterface.h"
#include <future> // std::promise, std::future
#include <list>

using json = nlohmann::json;

static std::string Name("webrtc70");

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

public:
	static void ClassInit();
	static void ClassCleanup();
	static const std::string& GetName();

	class Listener : public webrtc::PeerConnectionObserver
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

		std::future<json> GetFuture();

		/* Virtual methods inherited from webrtc::RTCStatsCollectorCallback. */
	public:
		void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

	private:
		std::promise<json> promise;
	};

public:
	explicit PeerConnection(Listener* listener, std::list<std::string> iceServerUris);
	~PeerConnection() = default;

	webrtc::PeerConnectionInterface::RTCConfiguration GetConfiguration() const;
	bool SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config);
	json GetNativeRtpCapabilities() const;
	std::future<std::string> CreateOffer(
	  const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);
	std::future<std::string> CreateAnswer(
	  const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);
	std::future<void> SetLocalDescription(PeerConnection::SdpType type, const std::string& sdp);
	std::future<void> SetRemoteDescription(PeerConnection::SdpType type, const std::string& sdp);
	const std::string GetLocalDescription();
	const std::string GetRemoteDescription();
	std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const;
	rtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(
	  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);
	void Close();
	rtc::scoped_refptr<webrtc::AudioSourceInterface> CreateAudioSource();
	rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(
	  const std::string& label, webrtc::AudioSourceInterface* source);
	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(
	  std::unique_ptr<cricket::VideoCapturer> capturer,
	  const webrtc::MediaConstraintsInterface* constraints = nullptr);
	rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(
	  const std::string& label, webrtc::VideoTrackSourceInterface* source);
	std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders();
	bool RemoveTrack(webrtc::RtpSenderInterface* sender);
	std::future<json> GetStats();
	std::future<json> GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface> selector);
	std::future<json> GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector);

private:
	// Listener.
	Listener* listener;

	// Signaling and worker threads.
	rtc::Thread* signalingThread;
	rtc::Thread* workerThread;

	// PeerConnection factory.
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory;

	// PeerConnection instance.
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc;
};

/* Inline methods */

inline const std::string& PeerConnection::GetName()
{
	return Name;
}

inline webrtc::PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() const
{
	return this->pc->GetConfiguration();
}

inline std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> PeerConnection::GetTransceivers() const
{
	return this->pc->GetTransceivers();
}

inline rtc::scoped_refptr<webrtc::AudioSourceInterface> PeerConnection::CreateAudioSource()
{
	cricket::AudioOptions options;

	return this->peerConnectionFactory->CreateAudioSource(options);
}

inline rtc::scoped_refptr<webrtc::AudioTrackInterface> PeerConnection::CreateAudioTrack(
  const std::string& label, webrtc::AudioSourceInterface* source)
{
	return this->peerConnectionFactory->CreateAudioTrack(label, source);
}

inline rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> PeerConnection::CreateVideoSource(
  std::unique_ptr<cricket::VideoCapturer> capturer,
  const webrtc::MediaConstraintsInterface* constraints)
{
	return this->peerConnectionFactory->CreateVideoSource(std::move(capturer), constraints);
}

inline rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnection::CreateVideoTrack(
  const std::string& label, webrtc::VideoTrackSourceInterface* source)
{
	return this->peerConnectionFactory->CreateVideoTrack(label, source);
}

inline std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> PeerConnection::GetSenders()
{
	return this->pc->GetSenders();
}

inline bool PeerConnection::RemoveTrack(webrtc::RtpSenderInterface* sender)
{
	return this->pc->RemoveTrack(sender);
}

inline void PeerConnection::Close()
{
	this->pc->Close();
}

/* SetSessionDescriptionObserver inline methods */

inline std::future<void> PeerConnection::SetSessionDescriptionObserver::GetFuture()
{
	return this->promise.get_future();
}

inline void PeerConnection::SetSessionDescriptionObserver::Reject(const std::string& error)
{
	this->promise.set_exception(std::make_exception_ptr(Exception(error)));
}

inline void PeerConnection::SetSessionDescriptionObserver::OnSuccess()
{
	this->promise.set_value();
};

/* CreateSessionDescriptionObserver inline methods */

inline std::future<std::string> PeerConnection::CreateSessionDescriptionObserver::GetFuture()
{
	return this->promise.get_future();
}

inline void PeerConnection::CreateSessionDescriptionObserver::Reject(const std::string& error)
{
	this->promise.set_exception(std::make_exception_ptr(Exception(error)));
}

inline void PeerConnection::CreateSessionDescriptionObserver::OnSuccess(
  webrtc::SessionDescriptionInterface* desc)
{
	std::string sdp;

	desc->ToString(&sdp);
	this->promise.set_value(sdp);
};

/* RTCStatsCollectorCallback inline methods */

inline void PeerConnection::RTCStatsCollectorCallback::OnStatsDelivered(
  const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
	std::string s = report->ToJson();

	// RtpReceiver stats JSON string is sometimes empty.
	if (s.empty())
		this->promise.set_value(json::array());
	else
		this->promise.set_value(json::parse(s));
};

inline std::future<json> PeerConnection::RTCStatsCollectorCallback::GetFuture()
{
	return this->promise.get_future();
}
} // namespace mediasoupclient

#endif
