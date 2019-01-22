#include "Transport.hpp"
#include "data/parameters.hpp"
#include "Logger.hpp"
#include "catch.hpp"
#include <stdexcept>

using namespace mediasoupclient;

class FakeSendTransportListener : public SendTransport::Listener
{
public:
	std::future<json> OnProduce(const json& producerLocalParameters) override
	{
		this->onProduceTimesCalled++;

		std::promise<json> promise;

		json producerRemoteParameters;

		auto kind = producerLocalParameters["kind"].get<std::string>();

		if (kind == "audio")
		{
			this->audioProducerLocalParameters = producerLocalParameters;
			this->audioProducerRemoteParameters = generateProducerRemoteParameters();
			producerRemoteParameters = this->audioProducerRemoteParameters;
		}
		else if (kind == "video")
		{
			this->videoProducerLocalParameters = producerLocalParameters;
			this->videoProducerRemoteParameters = generateProducerRemoteParameters();
			producerRemoteParameters = this->videoProducerRemoteParameters;
		}
		else
		{
			throw std::runtime_error("Unknown producerLocalParameters[\"kind\"]");
		}

		promise.set_value(producerRemoteParameters);

		return promise.get_future();
	};

	std::future<void> OnConnect(const json& transportLocalParameters) override
	{
		this->onConnectTimesCalled++;
		this->transportLocalParameters = transportLocalParameters;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	json transportLocalParameters;

	json audioProducerLocalParameters;
	json audioProducerRemoteParameters;
	json videoProducerLocalParameters;
	json videoProducerRemoteParameters;

	size_t onProduceTimesCalled{ 0 };
	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onProduceExpectedTimesCalled{ 0 };
	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };
};

class FakeRecvTransportListener : public Transport::Listener
{
public:
	std::future<void> OnConnect(const json& transportLocalParameters) override
	{
		this->transportLocalParameters = transportLocalParameters;
		this->onConnectTimesCalled++;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(const std::string& /*connectionState*/) override
	{
		this->onConnectionStateChangeTimesCalled++;
	};

public:
	json transportLocalParameters;

	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

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
