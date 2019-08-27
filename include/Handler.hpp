#ifndef MSC_HANDLER_HPP
#define MSC_HANDLER_HPP

#include "PeerConnection.hpp"
#include "sdp/RemoteSdp.hpp"
#include "json.hpp"
#include <map>
#include <memory>
#include <set>
#include <string>

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

	// Methods to be implemented by child classes.
public:
	virtual void RestartIce(const nlohmann::json& iceParameters) = 0;

public:
	static nlohmann::json GetNativeRtpCapabilities(
	  const PeerConnection::Options* peerConnectionOptions = nullptr);

public:
	explicit Handler(
	  PrivateListener* privateListener,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions);

	nlohmann::json GetTransportStats();
	void UpdateIceServers(const nlohmann::json& iceServerUris);

	void Close();

	/* Methods inherited from PeerConnectionListener. */
public:
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;

protected:
	void SetupTransport(
	  const std::string& localDtlsRole, nlohmann::json localSdpObject = nlohmann::json::object());

protected:
	// PrivateListener instance.
	PrivateListener* privateListener{ nullptr };

	// Remote SDP instance.
	std::unique_ptr<Sdp::RemoteSdp> remoteSdp;

	// Got transport local and remote parameters.
	bool transportReady{ false };

	// Map of RTCTransceivers indexed by MID.
	// @type {Map<String, RTCTransceiver>}
	std::unordered_map<std::string, webrtc::RtpTransceiverInterface*> mapMidTransceiver;

	// PeerConnection instance.
	std::unique_ptr<PeerConnection> pc;
};

class SendHandler : public Handler
{
public:
	SendHandler(
	  Handler::PrivateListener* privateListener,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const nlohmann::json& sendingRtpParametersByKind,
	  const nlohmann::json& sendingRemoteRtpParametersByKind = nlohmann::json());

	std::pair<std::string, nlohmann::json> Send(
	  webrtc::MediaStreamTrackInterface* track,
	  const std::vector<webrtc::RtpEncodingParameters>* encodings,
	  const nlohmann::json* codecOptions);
	void StopSending(const std::string& localId);
	void ReplaceTrack(const std::string& localId, webrtc::MediaStreamTrackInterface* track);
	void SetMaxSpatialLayer(const std::string& localId, uint8_t spatialLayer);
	nlohmann::json GetSenderStats(const std::string& localId);

	/* Methods inherided from Handler. */
public:
	void RestartIce(const nlohmann::json& iceParameters) override;

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
	RecvHandler(
	  Handler::PrivateListener* privateListener,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions);

	std::pair<std::string, webrtc::MediaStreamTrackInterface*> Receive(
	  const std::string& id, const std::string& kind, const nlohmann::json* rtpParameters);
	void StopReceiving(const std::string& localId);
	nlohmann::json GetReceiverStats(const std::string& localId);

	/* Methods inherided from Handler. */
public:
	void RestartIce(const nlohmann::json& iceParameters) override;

	// MID value counter. It must be incremented for each new m= section.
	uint32_t nextMid{ 0 };
};

/* Inline methods */

inline nlohmann::json Handler::GetTransportStats()
{
	return this->pc->GetStats();
}

inline void Handler::Close()
{
	this->pc->Close();
};
} // namespace mediasoupclient

#endif
