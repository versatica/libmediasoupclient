#include "DataProducer.hpp"
#include "Logger.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	DataProducer::DataProducer(
	  DataProducer::PrivateListener* privateListener,
	  DataProducer::Listener* listener,
	  const std::string& id,
	  rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel,
	  const json& sctpStreamParameters,
	  const json& appData)
	  : privateListener(privateListener), listener(listener), id(id),
	    webrtcDataChannel(webrtcDataChannel), sctpStreamParameters(sctpStreamParameters),
	    appData(appData)
	{
		MSC_TRACE();

		this->webrtcDataChannel->RegisterObserver(this);
	};

	const std::string& DataProducer::GetId() const
	{
		MSC_TRACE();

		return this->id;
	}

	std::string DataProducer::GetLocalId() const
	{
		MSC_TRACE();

		return std::to_string(this->webrtcDataChannel->id());
	}

	bool DataProducer::IsClosed() const
	{
		MSC_TRACE();

		return this->closed;
	}

	const json& DataProducer::GetSctpStreamParameters() const
	{
		MSC_TRACE();

		return this->sctpStreamParameters;
	}

	webrtc::DataChannelInterface::DataState DataProducer::GetReadyState()
	{
		MSC_TRACE();

		return this->webrtcDataChannel->state();
	}

	std::string DataProducer::GetLabel()
	{
		MSC_TRACE();

		return this->webrtcDataChannel->label();
	}

	std::string DataProducer::GetProtocol()
	{
		MSC_TRACE();

		return this->webrtcDataChannel->protocol();
	}

	uint64_t DataProducer::GetBufferedAmount() const
	{
		MSC_TRACE();

		return this->webrtcDataChannel->buffered_amount();
	}

	const json& DataProducer::GetAppData() const
	{
		MSC_TRACE();

		return this->appData;
	}

	void DataProducer::Close()
	{
		MSC_TRACE();

		if (this->closed)
			return;

		this->closed = true;
		this->webrtcDataChannel->Close();
		this->privateListener->OnClose(this);
	}

	void DataProducer::Send(const webrtc::DataBuffer& buffer)
	{
		MSC_TRACE();

		this->webrtcDataChannel->Send(buffer);
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
		this->webrtcDataChannel->Close();
		this->listener->OnTransportClose(this);
	}

	// The data channel state have changed.
	void DataProducer::OnStateChange()
	{
		MSC_TRACE();

		webrtc::DataChannelInterface::DataState state = this->webrtcDataChannel->state();

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
				// should we throw an exception?
				// MSC_THROW_ERROR("Unknown state", state); - cannot compile: Logger.hpp:103:23: error:
				// ‘MSC_CLASS’ was not declared in this scope
				break;
		}
	}

	//  A data buffer was successfully received.
	void DataProducer::OnMessage(const webrtc::DataBuffer& buffer)
	{
		// since this is a DataProducer, this method will never be called
	}

	// The data channel's buffered_amount has changed.
	void DataProducer::OnBufferedAmountChange(uint64_t sent_data_size)
	{
		MSC_TRACE();

		this->listener->OnBufferedAmountChange(this, sent_data_size);
	}
} // namespace mediasoupclient
