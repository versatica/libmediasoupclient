#include "Transport.hpp"

class FakeSendTransportListener : public SendTransport::Listener
{
public:
	std::future<json> OnProduce(const json& /*producerLocalParameters*/) override
	{
		this->onProduceTimesCalled++;

		std::promise<json> promise;

		json producerRemoteParameters = { { "id", "producer-id" } };

		promise.set_value(producerRemoteParameters);

		return promise.get_future();
	};

	void OnConnect(const json& /*transportLocalParameters*/) override
	{
		this->onConnectTimesCalled++;
	};

	void OnConnectionStateChange(const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	size_t onProduceTimesCalled{ 0 };
	size_t onStartConsumerTimesCalled{ 0 };
	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onProduceExpectedTimesCalled{ 0 };
	size_t onStartConsumerExpectedTimesCalled{ 0 };
	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };
};

class FakeRecvTransportListener : public RecvTransport::Listener
{
public:
	void OnStartConsumer(const Consumer* /*consumer*/) override
	{
		this->onStartConsumerTimesCalled++;
	};

	void OnConnect(const json& /*transportLocalParameters*/) override
	{
		this->onConnectTimesCalled++;
	};

	void OnConnectionStateChange(const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	size_t onStartConsumerTimesCalled{ 0 };
	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onStartConsumerExpectedTimesCalled{ 0 };
	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };
};

class FakeProducerPublicListener : public Producer::PublicListener
{
	public:
		void OnTransportClose() override
		{
			this->onTransportCloseTimesCalled++;
		}

	public:
		size_t onTransportCloseTimesCalled{ 0 };
		size_t onTransportCloseExpetecTimesCalled{ 0 };
};

class FakeConsumerPublicListener : public Consumer::PublicListener
{
	public:
		void OnTransportClose() override
		{
			this->onTransportCloseTimesCalled++;
		}

	public:
		size_t onTransportCloseTimesCalled{ 0 };
		size_t onTransportCloseExpetecTimesCalled{ 0 };
};
