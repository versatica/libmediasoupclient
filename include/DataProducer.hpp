#ifndef MSC_DATAPRODUCER_HPP
#define MSC_DATAPRODUCER_HPP

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
			virtual ~PrivateListener() = default;

			virtual void OnClose(DataProducer* dataProducer) = 0;
		};

		/* Public Listener API */
		class Listener
		{
		public:
			virtual ~Listener() = default;
			// DataChannel state changes.
			virtual void OnOpen(DataProducer* dataProducer) = 0;

			virtual void OnClose(DataProducer* dataProducer) = 0;

			virtual void OnBufferedAmountChange(DataProducer* dataProducer, uint64_t sentDataSize) = 0;

			virtual void OnTransportClose(DataProducer* dataProducer) = 0;
		};

	private:
		PrivateListener* const privateListener;
		Listener* const listener;
		const std::string id;
		const webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;
		bool closed{ false };
		const nlohmann::json sctpStreamParameters;
		const nlohmann::json appData;

		void TransportClosed();

		friend SendTransport;

	private:
		DataProducer(
		  DataProducer::PrivateListener* privateListener,
		  DataProducer::Listener* listener,
		  const std::string& id,
		  webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel,
		  const nlohmann::json& sctpStreamParameters,
		  const nlohmann::json& appData);

	public:
		/**
		 * DataProducer id.
		 */
		const std::string& GetId() const;

		/**
		 * Local id.
		 */
		std::string GetLocalId() const;

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
		std::string GetLabel();

		/**
		 * DataChannel protocol.
		 */
		std::string GetProtocol();

		/**
		 * DataChannel bufferedAmount.
		 */
		uint64_t GetBufferedAmount() const;

		/**
		 * App custom data.
		 */
		const nlohmann::json& GetAppData() const;

		/**
		 * Whether the DataProducer is closed.
		 */
		bool IsClosed() const;

		/**
		 * Closes the DataProducer.
		 *
		 * @remarks
		 * - The application owns the DataProducer, so it must destroy it after
		 *   closing it.
		 */
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
