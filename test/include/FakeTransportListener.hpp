#ifndef MSC_TEST_FAKE_TRANSPORT_LISTENER_HPP
#define MSC_TEST_FAKE_TRANSPORT_LISTENER_HPP

#include "fakeParameters.hpp"
#include "mediasoupclient.hpp"
#include <catch.hpp>

class FakeSendTransportListener : public mediasoupclient::SendTransport::Listener
{
public:
	virtual std::future<std::string> OnProduce(
	  mediasoupclient::SendTransport* /*transport*/,
	  const std::string& kind,
	  nlohmann::json rtpParameters,
	  const nlohmann::json& appData) override
	{
		this->onProduceTimesCalled++;

		std::promise<std::string> promise;

		json producerId;

		this->appData = appData;

		if (kind == "audio")
		{
			this->audioProducerLocalParameters = rtpParameters;
			this->audioProducerId              = generateProducerRemoteId();
			producerId                         = this->audioProducerId;
		}
		else if (kind == "video")
		{
			this->videoProducerLocalParameters = rtpParameters;
			this->videoProducerId              = generateProducerRemoteId();
			producerId                         = this->videoProducerId;
		}
		else
		{
			throw std::runtime_error("Unknown producerLocalParameters[\"kind\"]");
		}

		promise.set_value(producerId);

		return promise.get_future();
	};

	std::future<std::string> OnProduceData(
	  mediasoupclient::SendTransport* /*transport*/,
	  const nlohmann::json& /*sctpStreamParameters*/,
	  const std::string& /*label*/,
	  const std::string& /*protocol*/,
	  const nlohmann::json& appData) override
	{
		this->onProduceDataTimesCalled++;

		std::promise<std::string> promise;

		this->appData = appData;

		// this->audioProducerLocalParameters = rtpParameters;
		this->dataProducerId = generateProducerRemoteId();

		promise.set_value(this->dataProducerId);

		return promise.get_future();
	}

	std::future<void> OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters) override
	{
		this->onConnectTimesCalled++;
		this->id             = transport->GetId();
		this->dtlsParameters = dtlsParameters;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(
	  mediasoupclient::Transport* transport, const std::string& /*connectionState*/) override
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
	std::string dataProducerId;
	std::string dataConsumerId;

	size_t onProduceTimesCalled{ 0 };
	size_t onConnectTimesCalled{ 0 };
	size_t onConnectionStateChangeTimesCalled{ 0 };

	size_t onProduceExpectedTimesCalled{ 0 };
	size_t onConnectExpectedTimesCalled{ 0 };
	size_t onConnectionStateChangeExpectedTimesCalled{ 0 };

	size_t onProduceDataTimesCalled{ 0 };
	size_t onProduceDataExpectedTimesCalled{ 0 };
};

class FakeRecvTransportListener : public mediasoupclient::RecvTransport::Listener
{
public:
	std::future<void> OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters) override
	{
		this->id             = transport->GetId();
		this->dtlsParameters = dtlsParameters;
		this->onConnectTimesCalled++;

		std::promise<void> promise;

		promise.set_value();

		return promise.get_future();
	};

	void OnConnectionStateChange(
	  mediasoupclient::Transport* transport, const std::string& /*connectionState*/) override
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

class FakeProducerListener : public mediasoupclient::Producer::Listener,
                             public mediasoupclient::DataProducer::Listener
{
public:
	void OnOpen(mediasoupclient::DataProducer* dataProducer) override
	{
	}
	void OnClose(mediasoupclient::DataProducer* dataProducer) override{};
	void OnBufferedAmountChange(
	  mediasoupclient::DataProducer* dataProducer, uint64_t sent_data_size) override{};

	void OnTransportClose(mediasoupclient::DataProducer* /*dataProducer*/) override{};

	void OnTransportClose(mediasoupclient::Producer* /*producer*/) override
	{
		this->onTransportCloseTimesCalled++;
	}

public:
	size_t onTransportCloseTimesCalled{ 0 };
	size_t onTransportCloseExpectedTimesCalled{ 0 };
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
	size_t onTransportCloseExpectedTimesCalled{ 0 };
};

#endif
