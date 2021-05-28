#ifndef MSC_DATAPRODUCER_HPP
#define MSC_DATAPRODUCER_HPP

#include "Handler.hpp"
#include <json.hpp>
#include <api/data_channel_interface.h>
#include <string>

namespace mediasoupclient
{
	class SendTransport;

	class DataProducer : public webrtc::DataChannelObserver
	{
	public:
		class PrivateListener
		{
		public:
			virtual void OnClose(DataProducer* dataProducer) = 0;
		};

		/* Public Listener API */
		class Listener
		{
		public:
			// DataChannel state changes.
			virtual void OnOpen(DataProducer* dataProducer)                                        = 0;
			virtual void OnClose(DataProducer* dataProducer)                                       = 0;
			virtual void OnBufferedAmountChange(DataProducer* dataProducer, uint64_t sentDataSize) = 0;

			virtual void OnTransportClose(DataProducer* dataProducer) = 0;
		};

	private:
		PrivateListener* privateListener;
		Listener* listener;
		std::string id;
		rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;
		bool closed{ false };
		nlohmann::json sctpStreamParameters;
		nlohmann::json appData;
		void TransportClosed();

		friend SendTransport;

	private:
		DataProducer(
		  DataProducer::PrivateListener* privateListener,
		  DataProducer::Listener* listener,
		  const std::string& id,
		  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
		  const nlohmann::json& sctpStreamParameters,
		  const nlohmann::json& appData);

	public:
		const std::string& GetId() const;
		std::string GetLocalId() const;
		const nlohmann::json& GetSctpStreamParameters() const;
		webrtc::DataChannelInterface::DataState GetReadyState() const;
		std::string GetLabel();
		std::string GetProtocol();
		uint64_t GetBufferedAmount() const;
		const nlohmann::json& GetAppData() const;
		bool IsClosed() const;
		void Close();
		void Send(const webrtc::DataBuffer& buffer);

		/* Virtual methods inherited from webrtc::DataChannelObserver. */
	public:
		void OnStateChange() override;
		//  A data buffer was successfully received.
		void OnMessage(const webrtc::DataBuffer& buffer) override;
		// The data channel's buffered_amount has changed.
		void OnBufferedAmountChange(uint64_t sentDataSize) override;
	};
} // namespace mediasoupclient

#endif
