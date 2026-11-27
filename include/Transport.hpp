#ifndef MSC_TRANSPORT_HPP
#define MSC_TRANSPORT_HPP

#include "Consumer.hpp"
#include "DataConsumer.hpp"
#include "DataProducer.hpp"
#include "Handler.hpp"
#include "Producer.hpp"
#include <json.hpp>
#include <api/media_stream_interface.h>    // webrtc::MediaStreamTrackInterface
#include <api/peer_connection_interface.h> // webrtc::PeerConnectionInterface
#include <api/rtp_parameters.h>            // webrtc::RtpEncodingParameters
#include <future>
#include <map>
#include <memory> // unique_ptr
#include <string>

namespace mediasoupclient
{
	// Fast forward declarations.
	class Device;

	class Transport : public Handler::PrivateListener
	{
	public:
		/* Public Listener API */
		class Listener
		{
		public:
			virtual ~Listener() = default;

			virtual std::future<void> OnConnect(Transport* transport, const nlohmann::json& dtlsParameters) = 0;

			virtual void OnConnectionStateChange(Transport* transport, const std::string& connectionState) = 0;
		};

		/* Only child classes will create transport intances */
	protected:
		Transport(Listener* listener, const std::string& id, const nlohmann::json& appData);

	public:
		~Transport() override = default;

		/**
		 * Transport id.
		 */
		const std::string& GetId() const;

		/**
		 * Whether the Transport is closed.
		 */
		bool IsClosed() const;

		/**
		 * Connection state.
		 */
		const std::string& GetConnectionState() const;

		/**
		 * App custom data.
		 */
		nlohmann::json& GetAppData();

		/**
		 * Closes the Transport, along with all its Producers and Consumers.
		 *
		 * @remarks
		 * - The application owns the Transport, so it must destroy it after closing
		 *   it, and the same goes for the Producers and Consumers it created.
		 */
		virtual void Close();

		/**
		 * Get associated PeerConnection stats.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed.
		 */
		nlohmann::json GetStats() const;

		/**
		 * Restarts the ICE connection.
		 *
		 * @param iceParameters - New ICE parameters of the server side transport.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed.
		 */
		void RestartIce(const nlohmann::json& iceParameters);

		/**
		 * Updates the ICE servers to use.
		 *
		 * @param iceServers - ICE servers to use from now on.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed.
		 */
		void UpdateIceServers(const nlohmann::json& iceServers);

	protected:
		void SetHandler(Handler* handler);

		/* Pure virtual methods inherited from Handler::PrivateListener */
	public:
		void OnConnect(nlohmann::json& dtlsParameters) override;

		void OnConnectionStateChange(
		  webrtc::PeerConnectionInterface::IceConnectionState connectionState) override;

	protected:
		// Closed flag.
		bool closed{ false };
		// Whether the Consumer for RTP probation has been created.
		bool probatorConsumerCreated{ false };
		// Whether this transport supports DataChannel.
		bool hasSctpParameters{ false };

	private:
		// Listener.
		Listener* const listener;
		// Id.
		const std::string id;
		// Transport (IceConneciton) connection state.
		webrtc::PeerConnectionInterface::IceConnectionState connectionState{
			webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew
		};
		// Handler.
		Handler* handler{ nullptr };
		// App custom data.
		nlohmann::json appData = nlohmann::json::object();
	};

	class SendTransport : public Transport,
	                      public Producer::PrivateListener,
	                      public DataProducer::PrivateListener
	{
	public:
		/* Public Listener API */
		class Listener : public Transport::Listener
		{
		public:
			virtual std::future<std::string> OnProduce(
			  SendTransport* transport,
			  const std::string& kind,
			  nlohmann::json rtpParameters,
			  const nlohmann::json& appData) = 0;

			virtual std::future<std::string> OnProduceData(
			  SendTransport* transport,
			  const nlohmann::json& sctpStreamParameters,
			  const std::string& label,
			  const std::string& protocol,
			  const nlohmann::json& appData) = 0;
		};

	private:
		SendTransport(
		  Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions,
		  const std::function<nlohmann::json(nlohmann::json&)>& getSendExtendedRtpCapabilities,
		  const std::map<std::string, bool>* canProduceByKind,
		  const nlohmann::json& appData);

		/* Device is the only one constructing Transports. */
		friend Device;

	public:
		/**
		 * Creates a Producer that sends the given track.
		 *
		 * @param producerListener - Listener of the producer events.
		 * @param track - Track to send.
		 * @param encodings - Encodings to use, or no value for a single encoding.
		 * @param codecOptions - Codec specific options.
		 * @param codec - Codec to use instead of the preferred one.
		 * @param zeroRtpOnPause - Whether pausing the Producer also sets its
		 *   RtpTransceiver inactive, so that no RTP is sent at all.
		 * @param appData - Custom data to attach to the producer.
		 *
		 * @remarks
		 * - The returned Producer is owned by the application, which must close and
		 *   destroy it.
		 * - zeroRtpOnPause makes Producer::Pause() and Producer::Resume() run an
		 *   offer/answer cycle on the local PeerConnection, so they are not free and
		 *   may throw.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed or the
		 *   given track is ended.
		 * @throws MediaSoupClientTypeError if no track is given.
		 * @throws MediaSoupClientUnsupportedError if the Device cannot produce the
		 *   kind of the given track.
		 */
		[[nodiscard]] Producer* Produce(
		  Producer::Listener* producerListener,
		  webrtc::MediaStreamTrackInterface* track,
		  const std::vector<webrtc::RtpEncodingParameters>* encodings,
		  const nlohmann::json* codecOptions,
		  const nlohmann::json* codec,
		  bool zeroRtpOnPause           = false,
		  const nlohmann::json& appData = nlohmann::json::object());

		/**
		 * Creates a DataProducer that sends messages over SCTP.
		 *
		 * @param listener - Listener of the data producer events.
		 * @param label - Label of the underlying DataChannel.
		 * @param protocol - Subprotocol of the underlying DataChannel.
		 * @param ordered - Whether messages are delivered in order.
		 * @param maxRetransmits - Maximum number of retransmissions.
		 * @param maxPacketLifeTime - Time to retransmit a message for (ms).
		 * @param appData - Custom data to attach to the data producer.
		 *
		 * @remarks
		 * - The returned DataProducer is owned by the application, which must close
		 *   and destroy it.
		 * - maxRetransmits and maxPacketLifeTime are exclusive.
		 *
		 * @throws MediaSoupClientUnsupportedError if the server side transport does
		 *   not have SCTP enabled.
		 * @throws MediaSoupClientError if both maxRetransmits and maxPacketLifeTime
		 *   are given.
		 */
		[[nodiscard]] DataProducer* ProduceData(
		  DataProducer::Listener* listener,
		  const std::string& label      = "",
		  const std::string& protocol   = "",
		  bool ordered                  = true,
		  int maxRetransmits            = 0,
		  int maxPacketLifeTime         = 0,
		  const nlohmann::json& appData = nlohmann::json::object());

		/* Virtual methods inherited from Transport. */
	public:
		void Close() override;

		/* Virtual methods inherited from Producer::PrivateListener. */
	public:
		void OnClose(Producer* producer) override;

		void OnClose(DataProducer* dataProducer) override;

		void OnReplaceTrack(const Producer* producer, webrtc::MediaStreamTrackInterface* track) override;

		void OnSetMaxSpatialLayer(const Producer* producer, uint8_t maxSpatialLayer) override;

		nlohmann::json OnGetStats(const Producer* producer) override;

		void OnPause(Producer* producer) override;

		void OnResume(Producer* producer) override;

	private:
		// Listener instance.
		Listener* const listener;
		// Map of Producers indexed by id.
		std::unordered_map<std::string, Producer*> producers;
		std::unordered_map<std::string, DataProducer*> dataProducers;
		// Whether we can produce audio/video based on computed extended RTP
		// capabilities.
		const std::map<std::string, bool>* canProduceByKind;
		// SendHandler instance.
		const std::unique_ptr<SendHandler> sendHandler;
		// Maximum size for SCTP messages received by mediasoup DataProducers (in bytes).
		size_t maxReceiveMessageSize{ 0 };
	};

	class RecvTransport : public Transport,
	                      public Consumer::PrivateListener,
	                      public DataConsumer::PrivateListener
	{
	public:
		/* Public Listener API */
		class Listener : public Transport::Listener
		{
		};

	private:
		RecvTransport(
		  Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions,
		  const nlohmann::json* recvRtpCapabilities,
		  const nlohmann::json& appData);

		/* Device is the only one constructing Transports */
		friend Device;

	public:
		/**
		 * Creates a Consumer that receives a remote Producer.
		 *
		 * @param consumerListener - Listener of the consumer events.
		 * @param id - Consumer id given by the mediasoup Router.
		 * @param producerId - Id of the Producer being consumed.
		 * @param kind - "audio" or "video".
		 * @param rtpParameters - RTP parameters given by the mediasoup Router.
		 * @param appData - Custom data to attach to the consumer.
		 *
		 * @remarks
		 * - The returned Consumer is owned by the application, which must close and
		 *   destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed.
		 * @throws MediaSoupClientTypeError if any mandatory argument is missing.
		 */
		[[nodiscard]] Consumer* Consume(
		  Consumer::Listener* consumerListener,
		  const std::string& id,
		  const std::string& producerId,
		  const std::string& kind,
		  nlohmann::json* rtpParameters,
		  const nlohmann::json& appData = nlohmann::json::object());

		/**
		 * Creates a DataConsumer that receives messages over SCTP.
		 *
		 * @param listener - Listener of the data consumer events.
		 * @param id - DataConsumer id given by the mediasoup Router.
		 * @param producerId - Id of the DataProducer being consumed.
		 * @param streamId - SCTP stream id to receive on.
		 * @param label - Label of the underlying DataChannel.
		 * @param protocol - Subprotocol of the underlying DataChannel.
		 * @param appData - Custom data to attach to the data consumer.
		 *
		 * @remarks
		 * - The returned DataConsumer is owned by the application, which must close
		 *   and destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if the Transport is closed.
		 * @throws MediaSoupClientTypeError if any mandatory argument is missing or the
		 *   server side transport does not have SCTP enabled.
		 */
		[[nodiscard]] DataConsumer* ConsumeData(
		  DataConsumer::Listener* listener,
		  const std::string& id,
		  const std::string& producerId,
		  uint16_t streamId,
		  const std::string& label,
		  const std::string& protocol   = std::string(),
		  const nlohmann::json& appData = nlohmann::json::object());

		/* Virtual methods inherited from Transport. */
	public:
		void Close() override;

		/* Virtual methods inherited from Consumer::PrivateListener. */
	public:
		void OnClose(Consumer* consumer) override;

		void OnClose(DataConsumer* consumer) override;

		nlohmann::json OnGetStats(const Consumer* consumer) override;

		void OnPause(Consumer* consumer) override;

		void OnResume(Consumer* consumer) override;

	private:
		// Map of Consumers indexed by id.
		std::unordered_map<std::string, Consumer*> consumers;
		std::unordered_map<std::string, DataConsumer*> dataConsumers;
		// RecvHandler instance.
		const std::unique_ptr<RecvHandler> recvHandler;

		const nlohmann::json* recvRtpCapabilities;
		// Maximum size for SCTP messages sent by mediasoup DataConsumers (in bytes).
		size_t maxSendMessageSize{ 0 };
	};
} // namespace mediasoupclient
#endif
