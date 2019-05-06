#ifndef MSC_TRANSPORT_HPP
#define MSC_TRANSPORT_HPP

#include "Consumer.hpp"
#include "Handler.hpp"
#include "Producer.hpp"
#include "json.hpp"
#include "api/media_stream_interface.h"    // MediaStreamTrackInterface
#include "api/peer_connection_interface.h" // IceConnectionState
#include <future>
#include <map>
#include <memory> // unique_ptr
#include <string>
#include <utility>

namespace mediasoupclient
{
// Fast forward declarations.
class Device;

class Transport : public Handler::PrivateListener
{
public:
	/* Public Listener API */
	class Listener
	{
	public:
		virtual std::future<void> OnConnect(Transport* transport, const nlohmann::json& dtlsParameters) = 0;
		virtual void OnConnectionStateChange(Transport* transport, const std::string& connectionState) = 0;
	};

	/* Pure virtual methods inherited from Handler::PrivateListener */
public:
	void OnConnect(nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
	  webrtc::PeerConnectionInterface::IceConnectionState connectionState) override;

public:
	const std::string& GetId() const;
	const std::string& GetConnectionState() const;
	nlohmann::json& GetAppData();
	nlohmann::json GetStats() const;

	bool IsClosed() const;

	void RestartIce(const nlohmann::json& iceParameters);
	void UpdateIceServers(const nlohmann::json& iceServers);
	virtual void Close();

	/* Only child classes will create transport intances */
protected:
	Transport(
	  Listener* Listener,
	  const std::string& id,
	  const nlohmann::json* extendedRtpCapabilities,
	  const nlohmann::json& appData);

protected:
	void SetHandler(Handler* handler);

protected:
	// Closed flag.
	bool closed{ false };

	// Extended RTP capabilities.
	const nlohmann::json* extendedRtpCapabilities{ nullptr };

private:
	// Listener.
	Listener* listener{ nullptr };

	// Id.
	std::string id{};

	// Handler.
	Handler* handler{ nullptr };

	// Transport (IceConneciton) connection state.
	webrtc::PeerConnectionInterface::IceConnectionState connectionState{
		webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew
	};

	// App custom data.
	nlohmann::json appData = nlohmann::json::object();
};

class SendTransport : public Transport, public Producer::PrivateListener
{
public:
	/* Public Listener API */
	class Listener : public Transport::Listener
	{
	public:
		virtual std::future<std::string> OnProduce(
		  SendTransport* transport,
		  const std::string& kind,
		  nlohmann::json rtpParameters,
		  const nlohmann::json& appData) = 0;
	};

public:
	Producer* Produce(
	  Producer::Listener* producerListener,
	  webrtc::MediaStreamTrackInterface* track,
	  const std::vector<webrtc::RtpEncodingParameters>* encodings,
	  const nlohmann::json* codecOptions,
	  const nlohmann::json appData = nlohmann::json::object());

	/* Virtual methods inherited from Transport. */
public:
	void Close() override;

	/* Virtual methods inherited from Producer::PrivateListener. */
public:
	void OnClose(Producer* producer) override;
	void OnReplaceTrack(const Producer* producer, webrtc::MediaStreamTrackInterface* track) override;
	void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) override;
	nlohmann::json OnGetStats(const Producer* producer) override;

private:
	SendTransport(
	  Listener* listener,
	  const std::string& id,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const nlohmann::json* extendedRtpCapabilities,
	  const std::map<std::string, bool>* canProduceByKind,
	  const nlohmann::json& appData);

	/* Device is the only one constructing Transports */
	friend Device;

private:
	// Listener instance.
	Listener* listener;

	// Map of Producers indexed by id.
	std::unordered_map<std::string, Producer*> producers;

	// Whether we can produce audio/video based on computed extended RTP
	// capabilities.
	const std::map<std::string, bool>* canProduceByKind{ nullptr };

	// SendHandler instance.
	std::unique_ptr<SendHandler> handler;
};

class RecvTransport : public Transport, public Consumer::PrivateListener
{
public:
	using Listener = Transport::Listener;

	Consumer* Consume(
	  Consumer::Listener* consumerListener,
	  const std::string& id,
	  const std::string& producerId,
	  const std::string& kind,
	  const nlohmann::json* rtpParameters,
	  const nlohmann::json appData = nlohmann::json::object());

	/* Virtual methods inherited from Transport. */
public:
	void Close() override;

	/* Virtual methods inherited from Consumer::PrivateListener. */
public:
	void OnClose(Consumer* consumer) override;
	nlohmann::json OnGetStats(const Consumer* consumer) override;

private:
	RecvTransport(
	  Listener* listener,
	  const std::string& id,
	  const nlohmann::json& iceParameters,
	  const nlohmann::json& iceCandidates,
	  const nlohmann::json& dtlsParameters,
	  const PeerConnection::Options* peerConnectionOptions,
	  const nlohmann::json* extendedRtpCapabilities,
	  const nlohmann::json& appData);

	/* Device is the only one constructing Transports */
	friend Device;

private:
	// Map of Consumers indexed by id.
	std::unordered_map<std::string, Consumer*> consumers;

	// SendHandler instance.
	std::unique_ptr<RecvHandler> handler;
};

/* Transport instance inline methods */

inline Transport::Transport(
  Listener* listener,
  const std::string& id,
  const nlohmann::json* extendedRtpCapabilities,
  const nlohmann::json& appData)
  : extendedRtpCapabilities(extendedRtpCapabilities), listener(listener), id(id), appData(appData)
{
}

inline const std::string& Transport::GetId() const
{
	return this->id;
}

inline const std::string& Transport::GetConnectionState() const
{
	return PeerConnection::iceConnectionState2String[this->connectionState];
}

inline nlohmann::json& Transport::GetAppData()
{
	return this->appData;
}

inline nlohmann::json Transport::GetStats() const
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->handler->GetTransportStats();
}

inline bool Transport::IsClosed() const
{
	return this->closed;
}

inline void Transport::RestartIce(const nlohmann::json& iceParameters)
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->handler->RestartIce(iceParameters);
}

inline void Transport::UpdateIceServers(const nlohmann::json& iceServers)
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->handler->UpdateIceServers(iceServers);
}

inline void Transport::Close()
{
	if (this->closed)
		return;

	this->closed = true;

	// Close the handler.
	this->handler->Close();
}

inline void Transport::SetHandler(Handler* handler)
{
	this->handler = handler;
}

inline void Transport::OnConnectionStateChange(
  webrtc::PeerConnectionInterface::IceConnectionState connectionState)
{
	// Update connection state.
	this->connectionState = connectionState;

	return this->listener->OnConnectionStateChange(
	  this, PeerConnection::iceConnectionState2String[connectionState]);
}

/* SendTransport inline methods */

inline void SendTransport::Close()
{
	if (this->closed)
		return;

	Transport::Close();

	// Close all Producers.
	for (auto& kv : this->producers)
	{
		auto* producer = kv.second;

		producer->TransportClosed();
	}
}

/* RecvTransport inline methods */

inline void RecvTransport::Close()
{
	if (this->closed)
		return;

	Transport::Close();

	// Close all Producers.
	for (auto& kv : this->consumers)
	{
		auto* consumer = kv.second;

		consumer->TransportClosed();
	}
}
} // namespace mediasoupclient
#endif
