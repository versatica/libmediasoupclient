#include "mediasoupclient.hpp"
#include "data/parameters.hpp"
#include "catch.hpp"

class FakeSendTransportListener : public mediasoupclient::SendTransport::Listener
{
public:
		virtual std::future<std::string> OnProduce(mediasoupclient::SendTransport* /*transport*/, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json& appData) override
	{
		this->onProduceTimesCalled++;

		std::promise<std::string> promise;

		json producerId;

		this->appData = appData;

		if (kind == "audio")
		{
			this->audioProducerLocalParameters = rtpParameters;
			this->audioProducerId = generateProducerRemoteId();
			producerId = this->audioProducerId;
		}
		else if (kind == "video")
		{
			this->videoProducerLocalParameters = rtpParameters;
			this->videoProducerId = generateProducerRemoteId();
			producerId = this->videoProducerId;
		}
		else
		{
			throw std::runtime_error("Unknown producerLocalParameters[\"kind\"]");
		}

		promise.set_value(producerId);

		return promise.get_future();
	};

	std::future<void> OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters) override
	{
		this->onConnectTimesCalled++;
		this->id = transport->GetId();
		this->dtlsParameters = dtlsParameters;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	std::string id;
	json dtlsParameters;

	json audioProducerLocalParameters;
	std::string audioProducerId;
	json videoProducerLocalParameters;
	std::string videoProducerId;
	json appData;

	size_t onProduceTimesCalled{ 0 };
	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onProduceExpectedTimesCalled{ 0 };
	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };
};

class FakeRecvTransportListener : public mediasoupclient::RecvTransport::Listener
{
public:
	std::future<void> OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters) override
	{
		this->id = transport->GetId();
		this->dtlsParameters = dtlsParameters;
		this->onConnectTimesCalled++;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	std::string id;
	json dtlsParameters;

	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };
};

class FakeProducerListener : public mediasoupclient::Producer::Listener
{
	public:
		void OnTransportClose(mediasoupclient::Producer* /*producer*/) override
		{
			this->onTransportCloseTimesCalled++;
		}

	public:
		size_t onTransportCloseTimesCalled{ 0 };
		size_t onTransportCloseExpetecTimesCalled{ 0 };
};

class FakeConsumerListener : public mediasoupclient::Consumer::Listener
{
	public:
		void OnTransportClose(mediasoupclient::Consumer* /*consumer*/) override
		{
			this->onTransportCloseTimesCalled++;
		}

	public:
		size_t onTransportCloseTimesCalled{ 0 };
		size_t onTransportCloseExpetecTimesCalled{ 0 };
};
