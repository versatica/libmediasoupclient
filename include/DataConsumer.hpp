#ifndef MSC_DATACONSUMER_HPP
#define MSC_DATACONSUMER_HPP

#include <json.hpp>
#include <api/data_channel_interface.h>
#include <string>

namespace mediasoupclient
{
	class RecvTransport;

	class DataConsumer : public webrtc::DataChannelObserver
	{
	public:
		class PrivateListener
		{
		public:
			virtual void OnClose(DataConsumer* dataConsumer) = 0;
		};

		class Listener
		{
		public:
			// DataChannel state changes.
			virtual void OnConnecting(DataConsumer* dataConsumer) = 0;
			virtual void OnOpen(DataConsumer* dataConsumer)       = 0;
			virtual void OnClosing(DataConsumer* dataConsumer)    = 0;
			virtual void OnClose(DataConsumer* dataConsumer)      = 0;

			//  A data buffer was successfully received.
			virtual void OnMessage(DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer) = 0;

			virtual void OnTransportClose(DataConsumer* dataConsumer) = 0;
		};

	private:
		DataConsumer(
		  Listener* listener,
		  PrivateListener* privateListener,
		  const std::string& id,
		  const std::string& dataProducerId,
		  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
		  const nlohmann::json& sctpStreamParameters,
		  const nlohmann::json& appData);

	public:
		const std::string& GetId() const;
		std::string GetLocalId() const;
		const std::string& GetDataProducerId() const;
		const nlohmann::json& GetSctpStreamParameters() const;
		webrtc::DataChannelInterface::DataState GetReadyState() const;
		std::string GetLabel() const;
		std::string GetProtocol() const;
		const nlohmann::json& GetAppData() const;
		bool IsClosed() const;
		void Close();

	private:
		void TransportClosed();

		// RecvTransport will create instances and call private member TransporClosed.
		friend RecvTransport;

	private:
		Listener* listener;
		PrivateListener* privateListener;
		std::string id;
		std::string dataProducerId;
		rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;
		bool closed{ false };
		nlohmann::json sctpParameters;
		nlohmann::json appData;

		/* Virtual methods inherited from webrtc::DataChannelObserver. */
	public:
		// The data channel state has changed.
		void OnStateChange() override;
		//  A data buffer was successfully received.
		void OnMessage(const webrtc::DataBuffer& buffer) override;
		// The data channel's buffered_amount has changed.
		void OnBufferedAmountChange(uint64_t sentDataSize) override;
	};
} // namespace mediasoupclient

#endif
