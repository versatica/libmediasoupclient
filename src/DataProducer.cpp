#define MSC_CLASS "DataProducer"

#include "DataProducer.hpp"
#include "Logger.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	DataProducer::DataProducer(
	  DataProducer::PrivateListener* privateListener,
	  DataProducer::Listener* listener,
	  const std::string& id,
	  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
	  const json& sctpStreamParameters,
	  const json& appData)
	  : privateListener(privateListener), listener(listener), id(id), dataChannel(dataChannel),
	    sctpStreamParameters(sctpStreamParameters), appData(appData)
	{
		MSC_TRACE();

		this->dataChannel->RegisterObserver(this);
	};

	const std::string& DataProducer::GetId() const
	{
		MSC_TRACE();

		return this->id;
	}

	std::string DataProducer::GetLocalId() const
	{
		MSC_TRACE();

		return std::to_string(this->dataChannel->id());
	}

	const json& DataProducer::GetSctpStreamParameters() const
	{
		MSC_TRACE();

		return this->sctpStreamParameters;
	}

	webrtc::DataChannelInterface::DataState DataProducer::GetReadyState() const
	{
		MSC_TRACE();

		return this->dataChannel->state();
	}

	std::string DataProducer::GetLabel()
	{
		MSC_TRACE();

		return this->dataChannel->label();
	}

	std::string DataProducer::GetProtocol()
	{
		MSC_TRACE();

		return this->dataChannel->protocol();
	}

	uint64_t DataProducer::GetBufferedAmount() const
	{
		MSC_TRACE();

		return this->dataChannel->buffered_amount();
	}

	const json& DataProducer::GetAppData() const
	{
		MSC_TRACE();

		return this->appData;
	}

	bool DataProducer::IsClosed() const
	{
		MSC_TRACE();

		return this->closed;
	}

	void DataProducer::Close()
	{
		MSC_TRACE();

		if (this->closed)
			return;

		this->closed = true;
		this->dataChannel->Close();
		this->privateListener->OnClose(this);
	}

	void DataProducer::Send(const webrtc::DataBuffer& buffer)
	{
		MSC_TRACE();

		this->dataChannel->Send(buffer);
	}

	/**
	 * Transport was closed.
	 */
	void DataProducer::TransportClosed()
	{
		MSC_TRACE();

		if (this->closed)
			return;

		this->closed = true;
		this->dataChannel->Close();
		this->listener->OnTransportClose(this);
	}

	// The data channel state has changed.
	void DataProducer::OnStateChange()
	{
		MSC_TRACE();

		webrtc::DataChannelInterface::DataState state = this->dataChannel->state();

		switch (state)
		{
			case webrtc::DataChannelInterface::DataState::kConnecting:
				break;
			case webrtc::DataChannelInterface::DataState::kOpen:
				this->listener->OnOpen(this);
				break;
			case webrtc::DataChannelInterface::DataState::kClosing:
				break;
			case webrtc::DataChannelInterface::DataState::kClosed:
				this->listener->OnClose(this);
				break;
			default:
				MSC_ERROR("unknown state %s", webrtc::DataChannelInterface::DataStateString(state));
				break;
		}
	}

	//  A data buffer was successfully received.
	void DataProducer::OnMessage(const webrtc::DataBuffer& /*buffer*/)
	{
		MSC_ERROR("message received on DataProducer [dataProducer.id:%s]", this->GetId().c_str());
	}

	// The data channel's buffered_amount has changed.
	void DataProducer::OnBufferedAmountChange(uint64_t sentDataSize)
	{
		MSC_TRACE();

		this->listener->OnBufferedAmountChange(this, sentDataSize);
	}
} // namespace mediasoupclient
