#ifndef MSC_HANDLER_HPP
#define MSC_HANDLER_HPP

#include "PeerConnection.hpp"
#include "json.hpp"
#include "sdp/RemoteSdp.hpp"
#include <future>
#include <memory>
#include <set>
#include <string>

using json = nlohmann::json;

class Handler : public PeerConnection::Listener
{
public:
	class Listener
	{
	public:
		virtual std::future<void> OnConnect(json& transportLocalParameters) = 0;
		virtual void OnConnectionStateChange(
		  webrtc::PeerConnectionInterface::IceConnectionState connectionState) = 0;
	};

	// Methods to be implemented by child classes.
public:
	virtual std::future<void> RestartIce(const json& remoteIceParameters) = 0;

public:
	static json GetNativeRtpCapabilities();
	static const std::string& GetName();

public:
	explicit Handler(
	  Listener* listener,
	  const json& iceServers                = json::array(),
	  const std::string& iceTransportPolicy = "all",
	  const json& proprietaryConstraints    = json::object(),
	  json sendingRtpParametersByKind       = json::array());

	std::future<json> GetTransportStats();
	std::future<void> UpdateIceServers(const json& iceServerUris);

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
	json sendingRtpParametersByKind;

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
	  const json& transportRemoteParameters,
	  const json& iceServers,
	  const std::string& iceTransportPolicy,
	  const json& proprietaryConstraints,
	  const json& rtpParametersByKind);

	std::future<json> Send(webrtc::MediaStreamTrackInterface* track, uint8_t numStreams = 1);
	std::future<void> StopSending(webrtc::MediaStreamTrackInterface* track);
	std::future<void> ReplaceTrack(
	  webrtc::MediaStreamTrackInterface* track, webrtc::MediaStreamTrackInterface* newTrack);
	std::future<void> SetMaxSpatialLayer(
	  webrtc::MediaStreamTrackInterface* track, const std::string& spatialLayer);
	std::future<json> GetSenderStats(webrtc::MediaStreamTrackInterface* track);

	/* Methods inherided from Handler. */
public:
	std::future<void> RestartIce(const json& remoteIceParameters) override;

private:
	// Sending tracks.
	std::set<webrtc::MediaStreamTrackInterface*> tracks;
};

class RecvHandler : public Handler
{
public:
	RecvHandler(
	  Handler::Listener* listener,
	  const json& transportRemoteParameters,
	  const json& iceServers,
	  const std::string& iceTransportPolicy,
	  const json& proprietaryConstraints,
	  const json& rtpParametersByKind);

	std::future<webrtc::MediaStreamTrackInterface*> Receive(
	  const std::string& id, const std::string& kind, const json& rtpParameters);
	std::future<void> StopReceiving(const std::string& id);
	std::future<json> GetReceiverStats(const std::string& id);

	/* Methods inherided from Handler. */
public:
	std::future<void> RestartIce(const json& remoteIceParameters) override;

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
	std::map<const std::string, json> receiverInfos;
};

/* Inline methods */

inline std::future<json> Handler::GetTransportStats()
{
	return this->pc->GetStats();
}

inline void Handler::Close()
{
	this->pc->Close();
};

#endif
