#pragma once

#include "ProtooClient.hpp"
#include "mediasoupclient.hpp"
#include <api/media_stream_interface.h> // webrtc::VideoTrackInterface
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * RoomClient
 *
 * Wraps a ProtooClient WebSocket connection and a mediasoupclient Device to
 * implement the full mediasoup-demo room protocol:
 *
 *   - Loads the Device from the router's RTP capabilities.
 *   - Creates a bidirectional pair of WebRTC transports (send + recv).
 *   - Joins the room and handles server-initiated newConsumer / newDataConsumer
 *     requests as well as peerClosed / newPeer notifications.
 *   - Exposes simple enable/disable methods for mic, camera, and data chat.
 *
 * Caller wires up the three std::function callbacks before calling join().
 */
class RoomClient
{
public:
	// -------------------------------------------------------------------------
	// Public interface
	// -------------------------------------------------------------------------

	/**
	 * @param url             Full Protoo WebSocket URL, e.g.
	 *                        "wss://host:4443/?roomId=r&peerId=p"
	 * @param origin          HTTP Origin header sent during the WS handshake.
	 * @param displayName     Display name announced to the room on join().
	 * @param factory         PeerConnectionFactory used for all transports.
	 *                        Stored as a raw pointer — caller must keep it alive.
	 * @param pcOptions       Additional PeerConnection options (ICE servers, etc.).
	 *                        pcOptions.factory is overwritten by @p factory.
	 * @param onNewVideoTrack Called when a remote video track becomes available
	 *                        (newConsumer for kind=="video").
	 * @param onPeerLeft      Called when a peer leaves the room (peerClosed).
	 * @param onDataMessage   Called when a DataConsumer message is received.
	 */
	RoomClient(
	  const std::string& url,
	  const std::string& origin,
	  const std::string& displayName,
	  webrtc::PeerConnectionFactoryInterface* factory,
	  mediasoupclient::PeerConnection::Options pcOptions,
	  std::function<void(const std::string& peerId, webrtc::VideoTrackInterface*)> onNewVideoTrack,
	  std::function<void(const std::string& peerId)> onPeerLeft,
	  std::function<void(const std::string& message)> onDataMessage);

	~RoomClient();

	// Non-copyable, non-movable (owns raw transport/producer pointers).
	RoomClient(const RoomClient&)            = delete;
	RoomClient& operator=(const RoomClient&) = delete;
	RoomClient(RoomClient&&)                 = delete;
	RoomClient& operator=(RoomClient&&)      = delete;

	/**
	 * Connect to the Protoo server, load the Device, create transports, and
	 * send the "join" request.  Blocks until the join completes or throws on
	 * failure.  Call this once after setting the callbacks.
	 */
	void Join();

	/**
	 * Gracefully close all producers, consumers, transports, and the Protoo
	 * connection.  Safe to call more than once.
	 */
	void Close();

	/** Create an audio producer from the platform microphone. */
	void EnableMic();

	/** Close and destroy the current mic producer. */
	void DisableMic();

	/**
	 * Create a video producer from the supplied track.
	 * @param videoTrack A VideoTrackInterface obtained from the factory or a
	 *                   platform capture source (e.g. AVFCaptureVideoSource).
	 */
	void EnableCamera(webrtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack);

	/** Close and destroy the current camera producer. */
	void DisableCamera();

	/**
	 * Open a DataProducer on the send transport labelled "chat" with
	 * protocol "text/plain".  Messages from peers arrive via onDataMessage.
	 */
	void EnableChat();

private:
	// -------------------------------------------------------------------------
	// Private nested listener classes — full definitions live in RoomClient.cpp
	// -------------------------------------------------------------------------

	/** Handles send-transport signaling (connectWebRtcTransport + produce +
	 *  produceData) by forwarding to the Protoo request channel. */
	class SendTransportListener;

	/** Handles recv-transport signaling (connectWebRtcTransport). */
	class RecvTransportListener;

	/** Minimal Producer::Listener — only logs transport-close events. */
	class ProducerListener;

	/** Minimal Consumer::Listener — only logs transport-close events. */
	class ConsumerListener;

	/** Minimal DataProducer::Listener — forwards open/close/error. */
	class DataProducerListener;

	/**
	 * DataConsumer::Listener — delivers incoming messages to onDataMessage
	 * and handles state-change events.
	 */
	class DataConsumerListener;

	// -------------------------------------------------------------------------
	// Per-peer state
	// -------------------------------------------------------------------------

	struct Participant
	{
		mediasoupclient::Consumer* audioConsumer{ nullptr };
		mediasoupclient::Consumer* videoConsumer{ nullptr };
	};

	// -------------------------------------------------------------------------
	// Private helpers
	// -------------------------------------------------------------------------

	/**
	 * Dispatched by ProtooClient for server-initiated requests such as
	 * "newConsumer" and "newDataConsumer".  Fills @p responseData and may
	 * throw on error (ProtooClient will send the appropriate error response).
	 */
	void HandleRequest(const std::string& method, const json& data, json& responseData);

	/**
	 * Dispatched by ProtooClient for server push notifications such as
	 * "newPeer" and "peerClosed".
	 */
	void HandleNotification(const std::string& method, const json& data);

	// -------------------------------------------------------------------------
	// Data members
	// -------------------------------------------------------------------------

	// Protoo WebSocket client.
	ProtooClient protoo;

	// mediasoupclient Device — loaded with router RTP capabilities on join().
	mediasoupclient::Device device;

	// WebRTC transports — created during join(), owned by this class.
	mediasoupclient::SendTransport* sendTransport{ nullptr };
	mediasoupclient::RecvTransport* recvTransport{ nullptr };

	// Active producers — nullptr when the respective source is disabled.
	mediasoupclient::Producer* micProducer{ nullptr };
	mediasoupclient::Producer* cameraProducer{ nullptr };
	mediasoupclient::DataProducer* chatDataProducer{ nullptr };

	// Transport / producer / consumer listener singletons.
	// Defined as unique_ptr so that their full types only need to be visible
	// in RoomClient.cpp (incomplete-type friendly destruction).
	std::unique_ptr<SendTransportListener> sendListener;
	std::unique_ptr<RecvTransportListener> recvListener;
	std::unique_ptr<ConsumerListener> consumerListener;
	std::unique_ptr<ProducerListener> producerListener;
	std::unique_ptr<DataProducerListener> dataProducerListener;
	std::unique_ptr<DataConsumerListener> dataConsumerListener;

	// Remote participants — keyed by Protoo peerId.
	std::map<std::string, Participant> participants;

	// Guards participants: HandleRequest() runs on a dispatch worker thread,
	// HandleNotification() on the ixwebsocket receive thread, and Close() on
	// the main thread — all three can access participants concurrently.
	std::mutex participantsMutex;

	// Serialises HandleRequest() calls: multiple newConsumer/newDataConsumer
	// requests can arrive in quick succession and are each dispatched to a
	// separate worker thread by ProtooClient. Without serialisation both threads
	// can enter Consume() before the transport is marked ready, causing a second
	// connectWebRtcTransport call that the server rejects with "already called".
	std::mutex handleRequestMutex;

	// All active DataConsumers (owned by this class).
	std::vector<mediasoupclient::DataConsumer*> dataConsumers;

	// PeerConnection factory — not owned; caller must keep it alive.
	webrtc::PeerConnectionFactoryInterface* factory{ nullptr };

	// PeerConnection options forwarded to Device::Create*Transport().
	mediasoupclient::PeerConnection::Options pcOptions;

	// Display name announced to the room.
	std::string displayName;

	// Application callbacks.
	std::function<void(const std::string& peerId, webrtc::VideoTrackInterface*)> onNewVideoTrack;
	std::function<void(const std::string& peerId)> onPeerLeft;
	std::function<void(const std::string& message)> onDataMessage;

	// Set to true after close() is called to prevent double-close.
	bool closed{ false };
};
