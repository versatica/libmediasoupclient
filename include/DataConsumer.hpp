#ifndef MSC_DATACONSUMER_HPP
#define MSC_DATACONSUMER_HPP

#include <json.hpp>
#include <api/media_stream_interface.h> // webrtc::MediaStreamTrackInterface
#include <api/rtp_receiver_interface.h> // webrtc::RtpReceiverInterface
#include <api/data_channel_interface.h>
#include <string>

namespace mediasoupclient
{
	class RecvTransport;

	class DataConsumer : public webrtc::DataChannelObserver {
		public: 
		class PrivateListener
		{
			public:
				virtual void OnClose(DataConsumer* dataConsumer) = 0;
		};

		class Listener {
			public:
				// DataChannel state has changed
				virtual void OnConnecting(DataConsumer* dataConsumer) = 0;
				virtual void OnClosing(DataConsumer* dataConsumer) = 0;
				virtual void OnClose(DataConsumer* dataConsumer) = 0;
				virtual void OnOpen(DataConsumer* dataConsumer) = 0;

				//  A data buffer was successfully received.
				virtual void OnMessage(DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer) = 0;
				
				virtual void OnTransportClose(DataConsumer* dataConsumer) = 0;
		};

		public:
			DataConsumer(
				Listener* listener, 
				PrivateListener* privateListener,
				const std::string& id,
				const std::string& dataProducerId,
				rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel,
				const nlohmann::json& sctpStreamParameters,
				const nlohmann::json& appData);

			const std::string& GetId();
			std::string GetLocalId() const;
			const std::string& GetDataProducerId();
			const bool IsClosed();
			const nlohmann::json& GetSctpStreamParameters();
			webrtc::DataChannelInterface::DataState GetReadyState();
			std::string GetLabel();
			std::string GetProtocol();
			const nlohmann::json& GetAppData();
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
			rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel;
			bool closed;
			nlohmann::json sctpParameters;
			nlohmann::json appData;
			
		public:
			// From webrtc::DataChannelObserver
			// The data channel state has changed.
			void OnStateChange() override;
			//  A data buffer was successfully received.
			void OnMessage(const webrtc::DataBuffer& buffer) override;
			// The data channel's buffered_amount has changed.
			void OnBufferedAmountChange(uint64_t sent_data_size) override;
	};
} // namespace mediasoupclient

#endif
