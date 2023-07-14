#ifndef MSC_HANDLER_HPP
#define MSC_HANDLER_HPP

#include "MediaSoupClientErrors.hpp"
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
#include <optional>

namespace mediasoupclient
{
	class Handler : public PeerConnection::PrivateListener
	{
	public:
		class PrivateListener
		{
		public:
			virtual ~PrivateListener() = default;
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
		using CapabilityCallback = std::function<void(nlohmann::json, webrtc::RTCError)>;

		static void GetNativeRtpCapabilities(CapabilityCallback callback) {
			return GetNativeRtpCapabilities({}, callback);
		}

		static void GetNativeRtpCapabilities(const PeerConnection::Options& opts, CapabilityCallback callback);

		static nlohmann::json GetNativeSctpCapabilities();

	public:
		explicit Handler(
		  PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options& peerConnectionOptions);

	public:
		void Close();
		void GetTransportStats(PeerConnection::StatsHandler);
		void UpdateIceServers(const nlohmann::json& iceServerUris);
		virtual void RestartIce(const nlohmann::json& iceParameters) = 0;

		using DtlsParametersCallback = std::function<void(const nlohmann::json&, webrtc::RTCError)>;
		void GetDtlsParameters(DtlsParametersCallback);
	protected:
		void SetupTransport(const nlohmann::json& localSdpObject);

		virtual std::string DefaultLocalDtlsRole() = 0;
		virtual void CreateLocalSDP(PeerConnection::SDPHandler) = 0;
	private:
		std::string LocalDtlsRole();

		/* Methods inherited from PeerConnectionListener. */
	public:
		void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;

	protected:
		// PrivateListener instance.
		PrivateListener* privateListener{ nullptr };
		// Remote SDP instance.
		std::unique_ptr<Sdp::RemoteSdp> remoteSdp;
		// Got transport local and remote parameters.
		bool transportReady{ false };
		// Map of RTCTransceivers indexed by MID.
		std::unordered_map<std::string, rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> mapMidTransceiver{};
		// PeerConnection instance.
		std::unique_ptr<PeerConnection> pc;
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
			rtc::scoped_refptr<webrtc::RtpSenderInterface> rtpSender;
			nlohmann::json rtpParameters;
		};

	public:
		SendHandler(
		  Handler::PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options& peerConnectionOptions,
		  const nlohmann::json& sendingRtpParametersByKind,
		  const nlohmann::json& sendingRemoteRtpParametersByKind = nlohmann::json());

	public:
		void GetDtlsParameters(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track, std::vector<webrtc::RtpEncodingParameters>* encodings, DtlsParametersCallback callback);

		SendResult Send(
		  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
		  std::vector<webrtc::RtpEncodingParameters>* encodings,
		  const nlohmann::json* codecOptions,
		  const nlohmann::json* codec);
		void StopSending(const std::string& localId);
		void ReplaceTrack(const std::string& localId, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);
		void SetMaxSpatialLayer(const std::string& localId, uint8_t spatialLayer);
		void GetSenderStats(const std::string& localId, PeerConnection::StatsHandler);
		void RestartIce(const nlohmann::json& iceParameters) override;
		DataChannel SendDataChannel(const std::string& label, webrtc::DataChannelInit dataChannelInit);

	private:
		std::string DefaultLocalDtlsRole() override { return "server"; }
		void CreateLocalSDP(PeerConnection::SDPHandler fn) override { this->pc->CreateOffer(fn); }

	private:
		// Generic sending RTP parameters for audio and video.
		nlohmann::json sendingRtpParametersByKind;
		// Generic sending RTP parameters for audio and video suitable for the SDP
		// remote answer.
		nlohmann::json sendingRemoteRtpParametersByKind;
		// Set if DTLS parameters were extracted before setting up the transport
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> initialTransceiver;
	};

	class RecvHandler : public Handler
	{
	public:
		struct RecvResult
		{
			std::string localId;
			rtc::scoped_refptr<webrtc::RtpReceiverInterface> rtpReceiver;
			rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
		};
	public:
		RecvHandler(
		  Handler::PrivateListener* privateListener,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options& peerConnectionOptions);

		void GetDtlsParameters(const std::string& id, const std::string& kind, const nlohmann::json* rtpParameters, DtlsParametersCallback callback);

		RecvResult Receive(const std::string& id, const std::string& kind, const nlohmann::json* rtpParameters);
		void StopReceiving(const std::string& localId);
		void GetReceiverStats(const std::string& localId, PeerConnection::StatsHandler);
		void RestartIce(const nlohmann::json& iceParameters) override;
		DataChannel ReceiveDataChannel(const std::string& label, webrtc::DataChannelInit dataChannelInit);

	private:
		std::string DefaultLocalDtlsRole() override { return "client"; }
		void CreateLocalSDP(PeerConnection::SDPHandler fn) override { this->pc->CreateAnswer(fn); }

		struct ConsumerRef {
			ConsumerRef() = default;
			ConsumerRef(const std::string& id, const std::string& kind, const nlohmann::json& rtpParameters): id(id), kind(kind), rtpParameters(rtpParameters) {}

			std::string id;
			std::string kind;
			nlohmann::json rtpParameters;

			friend bool operator == (const ConsumerRef &lhs, const ConsumerRef &rhs) {
				return lhs.id == rhs.id && lhs.kind == rhs.kind && lhs.rtpParameters == rhs.rtpParameters;
			}

			friend bool operator != (const ConsumerRef &lhs, const ConsumerRef &rhs) {
				return !(lhs == rhs);
			}
		};

		struct RemoteOffer {
			ConsumerRef consumer;
			std::string localId;
			std::string sdp;
		};

		struct RemoteOffer RemoteOffer(const std::string& id, const std::string& kind, const nlohmann::json& rtpParameters);

		std::optional<struct RemoteOffer> initialOffer;
	};
} // namespace mediasoupclient

#endif
