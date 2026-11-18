#include "RoomClient.hpp"
#include <iostream>

// ---------------------------------------------------------------------------
// Private listener classes
// ---------------------------------------------------------------------------

class RoomClient::SendTransportListener : public mediasoupclient::SendTransport::Listener
{
public:
	SendTransportListener(ProtooClient& protoo, const std::string& transportId)
	  : protoo(protoo), transportId(transportId)
	{
	}

	std::future<void> OnConnect(mediasoupclient::Transport* /*transport*/, const json& dtlsParameters) override
	{
		std::promise<void> p;

		try
		{
			this->protoo.Request(
			  "connectWebRtcTransport",
			  // clang-format off
			  {
				{ "transportId",    this->transportId },
				{ "dtlsParameters", dtlsParameters    },
			  });
			// clang-format on

			p.set_value();
		}
		catch (const std::exception& e)
		{
			std::cerr << "[send] OnConnect failed: " << e.what() << "\n";

			p.set_exception(std::current_exception());
		}

		return p.get_future();
	}

	void OnConnectionStateChange(mediasoupclient::Transport* /*transport*/, const std::string& state) override
	{
		std::cout << "[send] connection state changed to: " << state << "\n";
	}

	std::future<std::string> OnProduce(
	  mediasoupclient::SendTransport* /*transport*/,
	  const std::string& kind,
	  json rtpParameters,
	  const json& appData) override
	{
		std::promise<std::string> p;

		try
		{
			auto res = this->protoo.Request(
			  "produce",
			  // clang-format off
			  {
				{ "transportId",   this->transportId },
				{ "kind",          kind              },
				{ "rtpParameters", rtpParameters     },
				{ "appData",       appData           },
			  });
			// clang-format on

			p.set_value(res["producerId"].get<std::string>());
		}
		catch (const std::exception& e)
		{
			std::cerr << "[send] OnProduce failed: " << e.what() << "\n";

			p.set_exception(std::current_exception());
		}

		return p.get_future();
	}

	std::future<std::string> OnProduceData(
	  mediasoupclient::SendTransport* /*transport*/,
	  const json& sctpStreamParameters,
	  const std::string& label,
	  const std::string& protocol,
	  const json& appData) override
	{
		std::promise<std::string> p;

		try
		{
			auto res = this->protoo.Request(
			  "produceData",
			  // clang-format off
			  {
				{ "transportId",          this->transportId    },
				{ "sctpStreamParameters", sctpStreamParameters },
				{ "label",                label                },
				{ "protocol",             protocol             },
				{ "appData",              appData              },
			  });
			// clang-format on

			p.set_value(res["dataProducerId"].get<std::string>());
		}
		catch (const std::exception& e)
		{
			std::cerr << "[send] OnProduceData failed: " << e.what() << "\n";

			p.set_exception(std::current_exception());
		}

		return p.get_future();
	}

private:
	ProtooClient& protoo;
	std::string transportId;
};

class RoomClient::RecvTransportListener : public mediasoupclient::RecvTransport::Listener
{
public:
	RecvTransportListener(ProtooClient& protoo, const std::string& transportId)
	  : protoo(protoo), transportId(transportId)
	{
	}

	std::future<void> OnConnect(mediasoupclient::Transport* /*transport*/, const json& dtlsParameters) override
	{
		std::promise<void> p;

		try
		{
			this->protoo.Request(
			  "connectWebRtcTransport",
			  // clang-format off
			  {
				{ "transportId",    this->transportId },
				{ "dtlsParameters", dtlsParameters    },
			  });
			// clang-format on

			p.set_value();
		}
		catch (const std::exception& e)
		{
			std::cerr << "[recv] OnConnect failed: " << e.what() << "\n";
			p.set_exception(std::current_exception());
		}

		return p.get_future();
	}

	void OnConnectionStateChange(mediasoupclient::Transport* /*transport*/, const std::string& state) override
	{
		std::cout << "[recv] connection state changed to: " << state << "\n";
	}

private:
	ProtooClient& protoo;
	std::string transportId;
};

class RoomClient::ProducerListener : public mediasoupclient::Producer::Listener
{
public:
	void OnTransportClose(mediasoupclient::Producer* /*producer*/) override
	{
	}
};

class RoomClient::ConsumerListener : public mediasoupclient::Consumer::Listener
{
public:
	void OnTransportClose(mediasoupclient::Consumer* /*consumer*/) override
	{
	}
};

class RoomClient::DataProducerListener : public mediasoupclient::DataProducer::Listener
{
public:
	void OnOpen(mediasoupclient::DataProducer* /*dataProducer*/) override
	{
	}
	void OnClose(mediasoupclient::DataProducer* /*dataProducer*/) override
	{
	}
	void OnBufferedAmountChange(
	  mediasoupclient::DataProducer* /*dataProducer*/, uint64_t /*sentDataSize*/) override
	{
	}
	void OnTransportClose(mediasoupclient::DataProducer* /*dataProducer*/) override
	{
	}
};

class RoomClient::DataConsumerListener : public mediasoupclient::DataConsumer::Listener
{
public:
	explicit DataConsumerListener(RoomClient& room) : room(room)
	{
	}

	void OnConnecting(mediasoupclient::DataConsumer* /*dataConsumer*/) override
	{
	}
	void OnOpen(mediasoupclient::DataConsumer* /*dataConsumer*/) override
	{
	}
	void OnClosing(mediasoupclient::DataConsumer* /*dataConsumer*/) override
	{
	}
	void OnClose(mediasoupclient::DataConsumer* /*dataConsumer*/) override
	{
	}
	void OnMessage(mediasoupclient::DataConsumer* /*dataConsumer*/, const webrtc::DataBuffer& buffer) override
	{
		this->room.onDataMessage(std::string(buffer.data.data<char>(), buffer.data.size()));
	}
	void OnTransportClose(mediasoupclient::DataConsumer* /*dataConsumer*/) override
	{
	}

private:
	RoomClient& room;
};

// ---------------------------------------------------------------------------
// RoomClient
// ---------------------------------------------------------------------------

RoomClient::RoomClient(
  const std::string& url,
  const std::string& origin,
  const std::string& displayName,
  webrtc::PeerConnectionFactoryInterface* factory,
  mediasoupclient::PeerConnection::Options pcOptions,
  std::function<void(const std::string& peerId, webrtc::VideoTrackInterface*)> onNewVideoTrack,
  std::function<void(const std::string& peerId)> onPeerLeft,
  std::function<void(const std::string& message)> onDataMessage)
  : protoo(
      url,
      origin,
      [this](const std::string& method, const json& data) -> void
      {
	      HandleNotification(method, data);
      },
      [this](const std::string& method, const json& data, json& responseData) -> void
      {
	      HandleRequest(method, data, responseData);
      }),
    displayName(displayName),
    factory(factory),
    pcOptions(std::move(pcOptions)),
    onNewVideoTrack(std::move(onNewVideoTrack)),
    onPeerLeft(std::move(onPeerLeft)),
    onDataMessage(std::move(onDataMessage))
{
	this->pcOptions.factory = this->factory;
}

RoomClient::~RoomClient()
{
	Close();
}

void RoomClient::Join()
{
	this->protoo.Connect();
	std::cout << "[room] connected\n";

	// -- Load device --
	auto res = this->protoo.Request("getRouterRtpCapabilities");

	this->device.Load(res["routerRtpCapabilities"], &this->pcOptions, false);
	std::cout << "[room] device loaded\n";

	// -- SendTransport --
	{
		auto r = this->protoo.Request(
		  "createWebRtcTransport",
		  // clang-format off
		  {
			{ "forceTcp", false                           },
			{ "appData",  { { "direction", "producer" } } },
		  });
		// clang-format on

		this->sendListener = std::make_unique<SendTransportListener>(this->protoo, r["transportId"]);
		this->sendTransport.reset(this->device.CreateSendTransport(
		  this->sendListener.get(),
		  r["transportId"],
		  r["iceParameters"],
		  r["iceCandidates"],
		  r["dtlsParameters"],
		  r["sctpParameters"],
		  std::addressof(this->pcOptions)));
	}

	// -- RecvTransport --
	{
		auto r = this->protoo.Request(
		  "createWebRtcTransport",
		  // clang-format off
		  {
			{ "forceTcp", false                           },
			{ "appData",  { { "direction", "consumer" } } },
		  });
		// clang-format on

		this->recvListener = std::make_unique<RecvTransportListener>(this->protoo, r["transportId"]);
		this->recvTransport.reset(this->device.CreateRecvTransport(
		  this->recvListener.get(),
		  r["transportId"],
		  r["iceParameters"],
		  r["iceCandidates"],
		  r["dtlsParameters"],
		  r["sctpParameters"],
		  std::addressof(this->pcOptions)));
	}

	// -- Wire up server requests and notifications --
	this->consumerListener     = std::make_unique<ConsumerListener>();
	this->producerListener     = std::make_unique<ProducerListener>();
	this->dataProducerListener = std::make_unique<DataProducerListener>();
	this->dataConsumerListener = std::make_unique<DataConsumerListener>(*this);

	// -- Join room --
	this->protoo.Request(
	  "join",
	  // clang-format off
	  {
		{ "displayName",      this->displayName                   },
		{ "device",           { { "name", "mediasoupclient" } }   },
		{ "rtpCapabilities",  this->device.GetRtpCapabilities()   },
	  });
	// clang-format on

	std::cout << "[room] joined\n";
}

void RoomClient::Close()
{
	if (this->closed)
	{
		return;
	}

	this->closed = true;

	std::cout << "[room] closing...\n";

	// NOTE: Close the transports first, each Close() calls TransportClosed() on
	// all its producers/consumers before tearing down the PeerConnection, so they
	// must still be alive here. Only once the transport is done are they reset
	// below, which is why the order of these resets matters.
	if (this->sendTransport)
	{
		this->sendTransport->Close();
		this->sendTransport.reset();
	}

	this->micProducer.reset();
	this->cameraProducer.reset();
	this->chatDataProducer.reset();

	if (this->recvTransport)
	{
		this->recvTransport->Close();
		this->recvTransport.reset();
	}

	// Delete consumer pointers after the transport has already notified them.
	{
		const std::scoped_lock lock(this->participantsMutex);

		// Destroying the participants destroys the consumers they own.
		this->participants.clear();
	}

	this->dataConsumers.clear();

	this->protoo.Close();
}

void RoomClient::EnableMic()
{
	const auto audioSource = this->factory->CreateAudioSource({});
	const auto audioTrack  = this->factory->CreateAudioTrack("mic", audioSource.get());

	this->micProducer.reset(this->sendTransport->Produce(
	  this->producerListener.get(), audioTrack.get(), nullptr, nullptr, {}));

	std::cout << "[room] mic producer: " << this->micProducer->GetId() << "\n";
}

void RoomClient::DisableMic()
{
	if (!this->micProducer)
	{
		return;
	}

	this->micProducer->Close();
	this->micProducer.reset();
}

void RoomClient::EnableCamera(const webrtc::scoped_refptr<webrtc::VideoTrackInterface>& videoTrack)
{
	if (!videoTrack)
	{
		return;
	}

	this->cameraProducer.reset(this->sendTransport->Produce(
	  this->producerListener.get(), videoTrack.get(), nullptr, nullptr, {}));

	std::cout << "[room] camera producer: " << this->cameraProducer->GetId() << "\n";
}

void RoomClient::DisableCamera()
{
	if (!this->cameraProducer)
	{
		return;
	}

	this->cameraProducer->Close();
	this->cameraProducer.reset();
}

void RoomClient::EnableChat()
{
	this->chatDataProducer.reset(this->sendTransport->ProduceData(
	  this->dataProducerListener.get(),
	  "chat",
	  "text/plain",
	  true,
	  0,
	  0,
	  // clang-format off
	  {
		{ "channel", "chat" },
	  }));
	// clang-format on

	std::cout << "[room] chat data producer: " << this->chatDataProducer->GetId() << "\n";
}

// ---------------------------------------------------------------------------
// Private handlers
// ---------------------------------------------------------------------------

void RoomClient::HandleRequest(const std::string& method, const json& data, json& /*responseData*/)
{
	const std::scoped_lock lock(this->handleRequestMutex);

	if (method == "newConsumer")
	{
		const auto consumerId = data["consumerId"].get<std::string>();
		const auto producerId = data["producerId"].get<std::string>();
		const auto kind       = data["kind"].get<std::string>();
		const auto peerIdFrom = data["peerId"].get<std::string>();
		auto rtpParameters    = data["rtpParameters"];

		auto* consumer = this->recvTransport->Consume(
		  this->consumerListener.get(), consumerId, producerId, kind, std::addressof(rtpParameters));

		{
			const std::scoped_lock lock(this->participantsMutex);
			auto& p = this->participants[peerIdFrom];

			if (kind == "audio")
			{
				p.audioConsumer.reset(consumer);
			}
			else if (kind == "video")
			{
				p.videoConsumer.reset(consumer);
			}
		}

		std::cout << "[room] consuming " << kind << " from " << peerIdFrom << "\n";

		if (kind == "video")
		{
			auto* track = static_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack());

			this->onNewVideoTrack(peerIdFrom, track);
		}
	}
	else if (method == "newDataConsumer")
	{
		auto* dc = this->recvTransport->ConsumeData(
		  this->dataConsumerListener.get(),
		  data["dataConsumerId"].get<std::string>(),
		  data["dataProducerId"].get<std::string>(),
		  static_cast<uint16_t>(data["sctpStreamParameters"]["streamId"].get<int>()),
		  data.value("label", ""),
		  data.value("protocol", ""),
		  data.value("appData", json::object()));

		this->dataConsumers.emplace_back(dc);
	}
}

void RoomClient::HandleNotification(const std::string& method, const json& data)
{
	if (method == "newPeer")
	{
		std::cout << "[room] new peer: " << data["id"].get<std::string>() << "\n";
	}
	else if (method == "peerClosed")
	{
		const auto peerId = data["peerId"].get<std::string>();
		{
			const std::scoped_lock lock(this->participantsMutex);
			const auto it = this->participants.find(peerId);

			if (it != this->participants.end())
			{
				it->second.audioConsumer.reset();
				it->second.videoConsumer.reset();
				this->participants.erase(it);
			}
		}

		std::cout << "[room] peer left: " << peerId << "\n";

		this->onPeerLeft(peerId);
	}
}
