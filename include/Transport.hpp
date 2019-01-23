#ifndef MSC_TRANSPORT_HPP
#define MSC_TRANSPORT_HPP

#include "Consumer.hpp"
#include "Handler.hpp"
#include "Producer.hpp"
#include "json.hpp"
#include "api/mediastreaminterface.h"    // MediaStreamTrackInterface
#include "api/peerconnectioninterface.h" // IceConnectionState
#include <future>
#include <map>
#include <memory> // unique_ptr
#include <string>
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
class Transport : public Handler::Listener
{
public:
	/* Public Listener API */
	class Listener
	{
	public:
		virtual std::future<void> OnConnect(const json& transportLocalParameters) = 0;
		virtual void OnConnectionStateChange(const std::string& connectionState)  = 0;
	};

	/* Pure virtual methods inherited from Handler::Listener */
public:
	void OnConnect(json& transportLocalParameters) override;
	void OnConnectionStateChange(
	  webrtc::PeerConnectionInterface::IceConnectionState connectionState) override;

public:
	Transport(
	  Listener* listener,
	  const json& transportRemoteParameters,
	  json extendedRtpCapabilities,
	  json appData);

	const std::string& GetId() const;
	const std::string& GetConnectionState() const;
	const json& GetAppData() const;
	json GetStats() const;

	bool IsClosed() const;

	void RestartIce(const json& remoteIceParameters);
	void UpdateIceServers(const json& iceServers);
	virtual void Close();

protected:
	void SetHandler(Handler* handler);

protected:
	// Closed flag.
	bool closed{ false };

	// Extended RTP capabilities.
	json extendedRtpCapabilities;

private:
	// Id.
	std::string id{};

	// Listener.
	Listener* listener{ nullptr };

	Handler* handler{ nullptr };

	// Transport (IceConneciton) connection state.
	webrtc::PeerConnectionInterface::IceConnectionState connectionState{
		webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew
	};

	// App custom data.
	json appData{};
};

class SendTransport : public Transport, public Producer::Listener
{
public:
	/* Public Listener API */
	class Listener : public Transport::Listener
	{
	public:
		virtual std::future<json> OnProduce(const json& producerLocalParameters) = 0;
	};

public:
	static const json DefaultSimulcast;

public:
	SendTransport(
	  Listener* listener,
	  const json& transportRemoteParameters,
	  const json& iceServers,
	  const std::string& iceTransportPolicy,
	  const json& proprietaryConstraints,
	  const json& extendedRtpCapabilities,
	  std::map<std::string, bool> canProduceByKind,
	  json appData = json::object());

	Producer* Produce(
	  Producer::PublicListener* producerPublicListener,
	  webrtc::MediaStreamTrackInterface* track,
	  json simulcast          = DefaultSimulcast,
	  uint8_t maxSpatialLayer = 0,
	  json appData            = json::object());

	/* Virtual methods inherited from Transport. */
public:
	void Close() override;

	/* Virtual methods inherited from Producer::Listener. */
public:
	void OnClose(Producer* producer) override;
	void OnReplaceTrack(const Producer* producer, webrtc::MediaStreamTrackInterface* newTrack) override;
	void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) override;
	json OnGetStats(const Producer* producer) override;

private:
	// Listener instance.
	Listener* listener;

	// Map of Producers indexed by id.
	std::map<std::string, Producer*> producers;

	// Whether we can produce audio/video based on computed extended RTP
	// capabilities.
	std::map<std::string, bool> canProduceByKind;

	// SendHandler instance.
	std::unique_ptr<SendHandler> handler;
};

class RecvTransport : public Transport, public Consumer::Listener
{
public:
	RecvTransport(
	  Transport::Listener* listener,
	  const json& transportRemoteParameters,
	  const json& iceServers,
	  const std::string& iceTransportPolicy,
	  const json& proprietaryConstraints,
	  const json& extendedRtpCapabilities,
	  json appData = json::object());

	Consumer* Consume(
	  Consumer::PublicListener* consumerPublicListener,
	  const json& consumerRemoteParameters,
	  json appData = json::object());

	/* Virtual methods inherited from Transport. */
public:
	void Close() override;

	/* Virtual methods inherited from Consumer::Listener. */
public:
	void OnClose(Consumer* consumer) override;
	json OnGetStats(const Consumer* consumer) override;

private:
	// Map of Consumers indexed by id.
	std::map<std::string, Consumer*> consumers;

	// SendHandler instance.
	std::unique_ptr<RecvHandler> handler;
};

/* Transport instance inline methods */

inline Transport::Transport(
  Listener* listener, const json& transportRemoteParameters, json extendedRtpCapabilities, json appData)
  : listener(listener), id(transportRemoteParameters["id"].get<std::string>()),
    extendedRtpCapabilities(std::move(extendedRtpCapabilities)), appData(std::move(appData))
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

inline const json& Transport::GetAppData() const
{
	// TODO: what's the compiler warning:
	// "access of moved variable appData"
	return this->appData;
}

inline json Transport::GetStats() const
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

inline void Transport::RestartIce(const json& remoteIceParameters)
{
	if (this->closed)
		throw Exception("Invalid state");
	else
		return this->handler->RestartIce(remoteIceParameters);
}

inline void Transport::UpdateIceServers(const json& iceServers)
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
	  PeerConnection::iceConnectionState2String[connectionState]);
}

/* SendTransport inline methods */

inline void SendTransport::Close()
{
	if (this->closed)
		return;

	Transport::Close();

	// Close all Producers.
	for (auto kv : this->producers)
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
	for (auto kv : this->consumers)
	{
		auto* consumer = kv.second;

		consumer->TransportClosed();
	}
}
} // namespace mediasoupclient
#endif
