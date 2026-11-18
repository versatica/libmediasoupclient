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
			virtual ~PrivateListener() = default;

			virtual void OnClose(DataConsumer* dataConsumer) = 0;
		};

		class Listener
		{
		public:
			virtual ~Listener() = default;
			// DataChannel state changes.
			virtual void OnConnecting(DataConsumer* dataConsumer) = 0;

			virtual void OnOpen(DataConsumer* dataConsumer) = 0;

			virtual void OnClosing(DataConsumer* dataConsumer) = 0;

			virtual void OnClose(DataConsumer* dataConsumer) = 0;

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
		  webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
		  const nlohmann::json& sctpStreamParameters,
		  const nlohmann::json& appData);

	public:
		/**
		 * DataConsumer id.
		 */
		const std::string& GetId() const;

		/**
		 * Local id.
		 */
		std::string GetLocalId() const;

		/**
		 * Associated DataProducer id.
		 */
		const std::string& GetDataProducerId() const;

		/**
		 * SCTP stream parameters.
		 */
		const nlohmann::json& GetSctpStreamParameters() const;

		/**
		 * DataChannel readyState.
		 */
		webrtc::DataChannelInterface::DataState GetReadyState() const;

		/**
		 * DataChannel label.
		 */
		std::string GetLabel() const;

		/**
		 * DataChannel protocol.
		 */
		std::string GetProtocol() const;

		/**
		 * App custom data.
		 */
		const nlohmann::json& GetAppData() const;

		/**
		 * Whether the DataConsumer is closed.
		 */
		bool IsClosed() const;

		/**
		 * Closes the DataConsumer.
		 *
		 * @remarks
		 * - The application owns the DataConsumer, so it must destroy it after
		 *   closing it.
		 */
		void Close();

	private:
		void TransportClosed();

		// RecvTransport will create instances and call private member TransporClosed.
		friend RecvTransport;

	private:
		Listener* const listener;
		PrivateListener* const privateListener;
		const std::string id;
		const std::string dataProducerId;
		const webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;
		bool closed{ false };
		const nlohmann::json sctpParameters;
		const nlohmann::json appData;

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
