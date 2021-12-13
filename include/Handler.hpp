#ifndef MSC_HANDLER_HPP
#define MSC_HANDLER_HPP

#include "PeerConnection.hpp"
#include "sdp/RemoteSdp.hpp"
#include <json.hpp>
#include <api/media_stream_interface.h>    // webrtc::MediaStreamTrackInterface
#include <api/peer_connection_interface.h> // webrtc::PeerConnectionInterface
#include <api/rtp_parameters.h>            // webrtc::RtpEncodingParameters
#include <api/rtp_receiver_interface.h>    // webrtc::RtpReceiverInterface
#include <api/rtp_sender_interface.h>      // webrtc::RtpSenderInterface
#include <api/rtp_transceiver_interface.h> // webrtc::RtpTransceiverInterface
#include <string>
#include <unordered_map>

namespace mediasoupclient
{
	class Handler : public PeerConnection::PrivateListener
	{
	public:
		class PrivateListener
		{
		public:
			virtual void OnConnect(nlohmann::json& dtlsParameters) = 0;
			virtual void OnConnectionStateChange(
			  webrtc::PeerConnectionInterface::IceConnectionState connectionState) = 0;
		};

	public:
		struct DataChannel
		{
			rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;
			nlohmann::json sctpStreamParameters;
		};

	public:
		static nlohmann::json GetNativeRtpCapabilities(
		  const PeerConnection::Options* peerConnectionOptions = nullptr);
		static nlohmann::json GetNativeSctpCapabilities();

	public:
		explicit Handler(
		  PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions);

	public:
		void Close();
		nlohmann::json GetTransportStats();
		void UpdateIceServers(const nlohmann::json& iceServerUris);
		virtual void RestartIce(const nlohmann::json& iceParameters) = 0;

	protected:
		void SetupTransport(const std::string& localDtlsRole, nlohmann::json& localSdpObject);

		/* Methods inherited from PeerConnectionListener. */
	public:
		void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;

	protected:
		// PrivateListener instance.
		PrivateListener* privateListener{ nullptr };
		// Remote SDP instance.
		std::unique_ptr<Sdp::RemoteSdp> remoteSdp{ nullptr };
		// Got transport local and remote parameters.
		bool transportReady{ false };
		// Map of RTCTransceivers indexed by MID.
		std::unordered_map<std::string, webrtc::RtpTransceiverInterface*> mapMidTransceiver{};
		// PeerConnection instance.
		std::unique_ptr<PeerConnection> pc{ nullptr };
		bool hasDataChannelMediaSection = false;
		uint32_t nextSendSctpStreamId   = 0;
		// Initial server side DTLS role. If not 'auto', it will force the opposite
		// value in client side.
		std::string forcedLocalDtlsRole;
	};

	class SendHandler : public Handler
	{
	public:
		struct SendResult
		{
			std::string localId;
			webrtc::RtpSenderInterface* rtpSender{ nullptr };
			nlohmann::json rtpParameters;
		};

	public:
		SendHandler(
		  Handler::PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions,
		  const nlohmann::json& sendingRtpParametersByKind,
		  const nlohmann::json& sendingRemoteRtpParametersByKind = nlohmann::json());

	public:
		SendResult Send(
		  webrtc::MediaStreamTrackInterface* track,
		  std::vector<webrtc::RtpEncodingParameters>* encodings,
		  const nlohmann::json* codecOptions,
		  const nlohmann::json* codec);
		void StopSending(const std::string& localId);
		void ReplaceTrack(const std::string& localId, webrtc::MediaStreamTrackInterface* track);
		void SetMaxSpatialLayer(const std::string& localId, uint8_t spatialLayer);
		nlohmann::json GetSenderStats(const std::string& localId);
		void RestartIce(const nlohmann::json& iceParameters) override;
		DataChannel SendDataChannel(const std::string& label, webrtc::DataChannelInit dataChannelInit);

	private:
		// Generic sending RTP parameters for audio and video.
		nlohmann::json sendingRtpParametersByKind;
		// Generic sending RTP parameters for audio and video suitable for the SDP
		// remote answer.
		nlohmann::json sendingRemoteRtpParametersByKind;
	};

	class RecvHandler : public Handler
	{
	public:
		struct RecvResult
		{
			std::string localId;
			webrtc::RtpReceiverInterface* rtpReceiver{ nullptr };
			webrtc::MediaStreamTrackInterface* track{ nullptr };
		};

	public:
		RecvHandler(
		  Handler::PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions);

		RecvResult Receive(
		  const std::string& id, const std::string& kind, const nlohmann::json* rtpParameters);
		void StopReceiving(const std::string& localId);
		nlohmann::json GetReceiverStats(const std::string& localId);
		void RestartIce(const nlohmann::json& iceParameters) override;
		DataChannel ReceiveDataChannel(const std::string& label, webrtc::DataChannelInit dataChannelInit);
	};
} // namespace mediasoupclient

#endif
