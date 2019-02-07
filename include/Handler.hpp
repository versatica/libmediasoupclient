#ifndef MSC_HANDLER_HPP
#define MSC_HANDLER_HPP

#include "PeerConnection.hpp"
#include "sdp/RemoteSdp.hpp"
#include "json.hpp"
#include <memory>
#include <set>
#include <string>

namespace mediasoupclient
{
class Handler : public PeerConnection::Listener
{
public:
	class Listener
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
	static nlohmann::json GetNativeRtpCapabilities();
	static const std::string& GetName();

public:
	explicit Handler(
	  Listener* listener,
	  PeerConnection::Options* peerConnectionOptions,
	  nlohmann::json sendingRtpParametersByKind = nlohmann::json::array());

	nlohmann::json GetTransportStats();
	void UpdateIceServers(const nlohmann::json& iceServerUris);

	void Close();

	/* Methods inherited from PeerConnectionListener. */
public:
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;

protected:
	void SetupTransport(const std::string& localDtlsRole);

protected:
	// Listener instance.
	Listener* listener{ nullptr };

	// Generic sending RTP parameters for audio and video.
	nlohmann::json sendingRtpParametersByKind;

	// Remote SDP instance.
	std::unique_ptr<Sdp::RemoteSdp> remoteSdp;

	// Got transport local and remote parameters.
	bool transportReady{ false };

	// PeerConnection instance.
	std::unique_ptr<PeerConnection> pc;
};

class SendHandler : public Handler
{
public:
	SendHandler(
	  Handler::Listener* listener,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  PeerConnection::Options* peerConnectionOptions,
	  const nlohmann::json& rtpParametersByKind);

	nlohmann::json Send(
	  webrtc::MediaStreamTrackInterface* track,
	  const std::vector<webrtc::RtpEncodingParameters>& encodings);
	void StopSending(webrtc::MediaStreamTrackInterface* track);
	void ReplaceTrack(
	  webrtc::MediaStreamTrackInterface* track, webrtc::MediaStreamTrackInterface* newTrack);
	void SetMaxSpatialLayer(webrtc::MediaStreamTrackInterface* track, uint8_t spatialLayer);
	nlohmann::json GetSenderStats(webrtc::MediaStreamTrackInterface* track);

	/* Methods inherided from Handler. */
public:
	void RestartIce(const nlohmann::json& iceParameters) override;

private:
	// Sending tracks.
	std::set<webrtc::MediaStreamTrackInterface*> tracks;
};

class RecvHandler : public Handler
{
public:
	RecvHandler(
	  Handler::Listener* listener,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  PeerConnection::Options* peerConnectionOptions);

	webrtc::MediaStreamTrackInterface* Receive(
	  const std::string& id, const std::string& kind, const nlohmann::json& rtpParameters);
	void StopReceiving(const std::string& id);
	nlohmann::json GetReceiverStats(const std::string& id);

	/* Methods inherided from Handler. */
public:
	void RestartIce(const nlohmann::json& iceParameters) override;

	/*
	 * Receiver infos indexed by id.
	 *
	 * Receiver info:
	 * - mid {String}
	 * - kind {String}
	 * - closed {Boolean}
	 * - trackId {String}
	 * - ssrc {Number}
	 * - rtxSsrc {Number}
	 * - cname {String}
	 */
	std::map<const std::string, nlohmann::json> receiverInfos;
};

/* Inline methods */

inline const std::string& Handler::GetName()
{
	return PeerConnection::GetName();
}

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
