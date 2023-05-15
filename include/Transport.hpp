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
		Transport(
		  Listener* listener,
		  const std::string& id,
		  const nlohmann::json* extendedRtpCapabilities,
		  const nlohmann::json& appData);

	public:
		virtual ~Transport() = default;
		const std::string& GetId() const;
		bool IsClosed() const;
		const std::string& GetConnectionState() const;
		nlohmann::json& GetAppData();
		virtual void Close();
		nlohmann::json GetStats() const;
		void RestartIce(const nlohmann::json& iceParameters);
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
		// Extended RTP capabilities.
		const nlohmann::json* extendedRtpCapabilities{ nullptr };
		// SCTP max message size if enabled, null otherwise.
		size_t maxSctpMessageSize{ 0u };
		// Whether the Consumer for RTP probation has been created.
		bool probatorConsumerCreated{ false };
		// Whether this transport supports DataChannel.
		bool hasSctpParameters{ false };

	private:
		// Listener.
		Listener* listener{ nullptr };
		// Id.
		std::string id;
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
		  const nlohmann::json* extendedRtpCapabilities,
		  const std::map<std::string, bool>* canProduceByKind,
		  const nlohmann::json& appData);

		/* Device is the only one constructing Transports. */
		friend Device;

	public:
		Producer* Produce(
		  Producer::Listener* producerListener,
		  webrtc::MediaStreamTrackInterface* track,
		  const std::vector<webrtc::RtpEncodingParameters>* encodings,
		  const nlohmann::json* codecOptions,
		  const nlohmann::json* codec,
		  const nlohmann::json& appData = nlohmann::json::object());

		DataProducer* ProduceData(
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

	private:
		// Listener instance.
		Listener* listener;
		// Map of Producers indexed by id.
		std::unordered_map<std::string, Producer*> producers;
		std::unordered_map<std::string, DataProducer*> dataProducers;
		// Whether we can produce audio/video based on computed extended RTP
		// capabilities.
		const std::map<std::string, bool>* canProduceByKind{ nullptr };
		// SendHandler instance.
		std::unique_ptr<SendHandler> sendHandler;
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
		  const nlohmann::json* extendedRtpCapabilities,
		  const nlohmann::json& appData);

		/* Device is the only one constructing Transports */
		friend Device;

	public:
		Consumer* Consume(
		  Consumer::Listener* consumerListener,
		  const std::string& id,
		  const std::string& producerId,
		  const std::string& kind,
		  nlohmann::json* rtpParameters,
		  const nlohmann::json& appData = nlohmann::json::object());

		DataConsumer* ConsumeData(
		  DataConsumer::Listener* listener,
		  const std::string& id,
		  const std::string& producerId,
		  const uint16_t streamId,
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

	private:
		// Map of Consumers indexed by id.
		std::unordered_map<std::string, Consumer*> consumers;
		std::unordered_map<std::string, DataConsumer*> dataConsumers;
		// SendHandler instance.
		std::unique_ptr<RecvHandler> recvHandler;
	};
} // namespace mediasoupclient
#endif
