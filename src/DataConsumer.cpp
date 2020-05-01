#include "DataConsumer.hpp"
#include "Logger.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
    DataConsumer::DataConsumer(
        DataConsumer::Listener* listener, 
        DataConsumer::PrivateListener* privateListener,
        const std::string& id,
        const std::string& dataProducerId,
        rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel,
        const json& sctpStreamParameters,
        const json& appData):
            listener(listener), privateListener(privateListener), id(id), dataProducerId(dataProducerId),
            webrtcDataChannel(webrtcDataChannel), sctpParameters(sctpStreamParameters), 
            appData(appData)
    {
        MSC_TRACE();
        this->webrtcDataChannel->RegisterObserver(this);
    }

    // From webrtc::DataChannelObsever
    // The data channel state has changed.
    void DataConsumer::OnStateChange() {
        MSC_TRACE();
        
        this->listener->OnStateChange(this);
    }

    //  A data buffer was successfully received.
    void DataConsumer::OnMessage(const webrtc::DataBuffer& buffer) {
        MSC_TRACE();
        
        this->listener->OnMessage(this, buffer);
    }
  
    // The data channel's buffered_amount has changed.
    void DataConsumer::OnBufferedAmountChange(uint64_t sent_data_size) {
        MSC_TRACE();
        // should not happen on consumer (I guess)
    }

    /**
     * DataConsumer id.
     */
    const std::string& DataConsumer::GetId() {
        MSC_TRACE();
        
        return this->id;
    }

    std::string DataConsumer::GetLocalId() const {
        MSC_TRACE();

        return std::to_string(this->webrtcDataChannel->id());
    }
    /**
     * Associated DataProducer id.
     */
    const std::string& DataConsumer::GetDataProducerId() {
        MSC_TRACE();
        
        return this->dataProducerId;
    }
    /**
     * Whether the DataConsumer is closed.
     */
    const bool DataConsumer::IsClosed() {
        MSC_TRACE();
        
        return this->closed;
    }
    /**
     * SCTP stream parameters.
     */
    const json& DataConsumer::GetSctpStreamParameters() {
        MSC_TRACE();
        
        return this->sctpParameters;
    }
    /**
     * DataChannel readyState.
     */
    webrtc::DataChannelInterface::DataState DataConsumer::GetReadyState() {
        MSC_TRACE();
        return this->webrtcDataChannel->state();
    }
    /**
     * DataChannel label.
     */
    std::string DataConsumer::GetLabel() {
        return this->webrtcDataChannel->label();
    }
    /**
     * DataChannel protocol.
     */
    std::string DataConsumer::GetProtocol()
    {
        MSC_TRACE();
        return this->webrtcDataChannel->protocol();
    }
    /**
     * App custom data.
     */
    const json& DataConsumer::GetAppData() {
        MSC_TRACE();
        return this->appData;
    }

    /**
     * Closes the DataConsumer.
     */
    void DataConsumer::Close() {
        if (this->closed)
            return;
        MSC_TRACE();
        this->closed = true;
        this->webrtcDataChannel->Close();
        this->privateListener->OnClose(this);
    }
    /**
     * Transport was closed.
     */
    void DataConsumer::TransportClosed() {
        if (this->closed)
            return;
        MSC_TRACE();
        this->closed = true;
        this->webrtcDataChannel->Close();
        this->listener->OnTransportClosed(this);
    }
}
