#define MSC_CLASS "DataConsumer"

#include "DataConsumer.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	DataConsumer::DataConsumer(
	  DataConsumer::Listener* listener,
	  DataConsumer::PrivateListener* privateListener,
	  const std::string& id,
	  const std::string& dataProducerId,
	  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
	  const json& sctpStreamParameters,
	  const json& appData)
	  : listener(listener), privateListener(privateListener), id(id), dataProducerId(dataProducerId),
	    dataChannel(dataChannel), sctpParameters(sctpStreamParameters), appData(appData)
	{
		MSC_TRACE();

		this->dataChannel->RegisterObserver(this);
	}

	// The data channel state has changed.
	void DataConsumer::OnStateChange()
	{
		MSC_TRACE();

		webrtc::DataChannelInterface::DataState state = this->dataChannel->state();

		switch (state)
		{
			case webrtc::DataChannelInterface::DataState::kConnecting:
				this->listener->OnConnecting(this);
				break;
			case webrtc::DataChannelInterface::DataState::kOpen:
				this->listener->OnOpen(this);
				break;
			case webrtc::DataChannelInterface::DataState::kClosing:
				this->listener->OnClosing(this);
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
	void DataConsumer::OnMessage(const webrtc::DataBuffer& buffer)
	{
		MSC_TRACE();

		this->listener->OnMessage(this, buffer);
	}

	// The data channel's buffered_amount has changed.
	void DataConsumer::OnBufferedAmountChange(uint64_t /*sentDataSize*/)
	{
		MSC_TRACE();
		// Should not happen on consumer.
	}

	/**
	 * DataConsumer id.
	 */
	const std::string& DataConsumer::GetId() const
	{
		MSC_TRACE();

		return this->id;
	}

	std::string DataConsumer::GetLocalId() const
	{
		MSC_TRACE();

		return std::to_string(this->dataChannel->id());
	}

	/**
	 * Associated DataProducer id.
	 */
	const std::string& DataConsumer::GetDataProducerId() const
	{
		MSC_TRACE();

		return this->dataProducerId;
	}

	/**
	 * SCTP stream parameters.
	 */
	const json& DataConsumer::GetSctpStreamParameters() const
	{
		MSC_TRACE();

		return this->sctpParameters;
	}

	/**
	 * DataChannel readyState.
	 */
	webrtc::DataChannelInterface::DataState DataConsumer::GetReadyState() const
	{
		MSC_TRACE();

		return this->dataChannel->state();
	}

	/**
	 * DataChannel label.
	 */
	std::string DataConsumer::GetLabel() const
	{
		MSC_TRACE();

		return this->dataChannel->label();
	}

	/**
	 * DataChannel protocol.
	 */
	std::string DataConsumer::GetProtocol() const
	{
		MSC_TRACE();

		return this->dataChannel->protocol();
	}

	/**
	 * App custom data.
	 */
	const json& DataConsumer::GetAppData() const
	{
		MSC_TRACE();

		return this->appData;
	}

	/**
	 * Whether the DataConsumer is closed.
	 */
	bool DataConsumer::IsClosed() const
	{
		MSC_TRACE();

		return this->closed;
	}

	/**
	 * Closes the DataConsumer.
	 */
	void DataConsumer::Close()
	{
		MSC_TRACE();

		if (this->closed)
			return;

		this->closed = true;
		this->dataChannel->Close();
		this->privateListener->OnClose(this);
	}

	/**
	 * Transport was closed.
	 */
	void DataConsumer::TransportClosed()
	{
		MSC_TRACE();

		if (this->closed)
			return;

		this->closed = true;
		this->dataChannel->Close();
		this->listener->OnTransportClose(this);
	}
} // namespace mediasoupclient
