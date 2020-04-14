#ifndef MSC_DATACONSUMER_HPP
#define MSC_DATACONSUMER_HPP

#include <json.hpp>
#include <api/media_stream_interface.h> // webrtc::MediaStreamTrackInterface
#include <api/rtp_receiver_interface.h> // webrtc::RtpReceiverInterface
#include <api/data_channel_interface.h>
#include <string>

namespace mediasoupclient
{
	class DataConsumer : public webrtc::DataChannelObserver {
		public: 
		class PrivateListener
		{
			public:
				virtual void OnClose(DataConsumer* dataConsumer) = 0;
		};

		class Listener {
			public:
				// The data channel state have changed.
				virtual void OnStateChange(DataConsumer* dataConsumer) { };
				//  A data buffer was successfully received.
				virtual void OnMessage(DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer) = 0;
				// The data channel's buffered_amount has changed.
				// virtual void OnBufferedAmountChange(uint64_t sent_data_size) {}
				//
				virtual void OnTransportClosed(DataConsumer* dataConsumer) { };
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
			void TransportClosed();
		
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
			// From webrtc::DataChannelObsever
			// The data channel state have changed.
			void OnStateChange() override;
			//  A data buffer was successfully received.
			void OnMessage(const webrtc::DataBuffer& buffer) override;
			// The data channel's buffered_amount has changed.
			void OnBufferedAmountChange(uint64_t sent_data_size) override;
	};
} // namespace mediasoupclient

#endif
