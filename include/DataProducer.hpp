#ifndef MSC_DATAPRODUCER_HPP
#define MSC_DATAPRODUCER_HPP

#include <json.hpp>
#include <string>
#include <api/data_channel_interface.h>
#include <api/stats/rtcstats_objects.h>
#include <third_party/abseil-cpp/absl/types/optional.h>
#include "Handler.hpp"

namespace mediasoupclient
{
	// class Transport;
	class SendTransport;

	class DataProducer : public webrtc::DataChannelObserver {
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
			virtual void OnTransportClose(DataProducer* dataProducer) {};
			virtual void OnBufferedAmountChange(DataProducer* dataProducer, uint64_t sent_data_size) {};
			// virtual void OnOpen(DataProducer* dataProducer) = 0;
			// virtual void OnError(DataProducer* dataProducer) = 0;
			// virtual void OnClose(DataProducer* dataProducer) = 0;
		};


		private:
			PrivateListener* privateListener;
			Listener* listener;
    		std::string id;
			// std::string _localId;
    		rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel;
    		bool closed;
    		nlohmann::json sctpStreamParameters;
    		nlohmann::json appData;
			void TransportClosed();

			friend SendTransport;

		public:
			DataProducer(
				DataProducer::PrivateListener* privateListener,
				DataProducer::Listener* listener,
				const std::string& id,
				// const std::string& localId,
				rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel, 
				const nlohmann::json& sctpStreamParameters, 
				const nlohmann::json& appData);
			const std::string& GetId() const;
			std::string GetLocalId() const;
			// const std::string& GetLocalId() const;
			bool IsClosed() const;
			const nlohmann::json& GetSctpStreamParameters() const;
			webrtc::DataChannelInterface::DataState GetReadyState(); // I would call this GetDataState()
			std::string GetLabel();
			std::string GetProtocol();
			uint64_t GetBufferedAmount() const;
			uint64_t BufferedAmountLowThreshold;
			const nlohmann::json& GetAppData() const;
			void Close();
			void Send(const webrtc::DataBuffer& buffer);
		
		// from DataChannelObserver
		public:
			void OnStateChange() override;
  			//  A data buffer was successfully received.
  			void OnMessage(const webrtc::DataBuffer& buffer) override;
  			// The data channel's buffered_amount has changed.
  			void OnBufferedAmountChange(uint64_t sent_data_size) override;
	};
} // namespace mediasoupclient

#endif
