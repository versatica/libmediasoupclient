#ifndef MSC_DEVICE_HPP
#define MSC_DEVICE_HPP

#include "Transport.hpp"
#include <json.hpp>
#include <map>
#include <string>

namespace mediasoupclient
{
	class Device
	{
	public:
		Device() = default;

		~Device() = default;

		/**
		 * Whether the Device is loaded.
		 */
		bool IsLoaded() const;

		/**
		 * RTP capabilities of the Device for receiving media.
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 */
		const nlohmann::json& GetRtpCapabilities() const;

		/**
		 * Initialize the Device.
		 *
		 * @param routerRtpCapabilities - RTP capabilities of the mediasoup Router.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 *
		 * @throws MediaSoupClientInvalidStateError if already loaded.
		 * @throws MediaSoupClientTypeError if the given capabilities are invalid.
		 */
		[[deprecated("Use Load(routerRtpCapabilities, peerConnectionOptions, preferLocalCodecsOrder)")]]
		void Load(
		  nlohmann::json routerRtpCapabilities,
		  const PeerConnection::Options* peerConnectionOptions = nullptr);

		/**
		 * Initialize the Device.
		 *
		 * @param routerRtpCapabilities - RTP capabilities of the mediasoup Router.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 * @param preferLocalCodecsOrder - Whether the codecs are ordered as the local
		 *   capabilities do instead of as the Router ones.
		 *
		 * @throws MediaSoupClientInvalidStateError if already loaded.
		 * @throws MediaSoupClientTypeError if the given capabilities are invalid.
		 */
		void Load(
		  nlohmann::json routerRtpCapabilities,
		  const PeerConnection::Options* peerConnectionOptions,
		  bool preferLocalCodecsOrder);

		/**
		 * Whether we can produce media of the given kind.
		 *
		 * @param kind - "audio" or "video".
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 * @throws MediaSoupClientTypeError if the kind is not "audio" nor "video".
		 */
		bool CanProduce(const std::string& kind);

		/**
		 * Creates a Transport for sending media.
		 *
		 * @param listener - Listener of the transport events.
		 * @param id - Transport id given by the mediasoup Router.
		 * @param iceParameters - ICE parameters of the server side transport.
		 * @param iceCandidates - ICE candidates of the server side transport.
		 * @param dtlsParameters - DTLS parameters of the server side transport.
		 * @param sctpParameters - SCTP parameters of the server side transport.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 * @param appData - Custom data to attach to the transport.
		 *
		 * @remarks
		 * - The returned transport is owned by the application, which must close and
		 *   destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 * @throws MediaSoupClientTypeError if appData is not a JSON object.
		 */
		[[nodiscard]] SendTransport* CreateSendTransport(
		  SendTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;

		/**
		 * Creates a Transport for sending media.
		 *
		 * @param listener - Listener of the transport events.
		 * @param id - Transport id given by the mediasoup Router.
		 * @param iceParameters - ICE parameters of the server side transport.
		 * @param iceCandidates - ICE candidates of the server side transport.
		 * @param dtlsParameters - DTLS parameters of the server side transport.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 * @param appData - Custom data to attach to the transport.
		 *
		 * @remarks
		 * - The returned transport is owned by the application, which must close and
		 *   destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 * @throws MediaSoupClientTypeError if appData is not a JSON object.
		 */
		[[nodiscard]] SendTransport* CreateSendTransport(
		  SendTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;

		/**
		 * Creates a Transport for receiving media.
		 *
		 * @param listener - Listener of the transport events.
		 * @param id - Transport id given by the mediasoup Router.
		 * @param iceParameters - ICE parameters of the server side transport.
		 * @param iceCandidates - ICE candidates of the server side transport.
		 * @param dtlsParameters - DTLS parameters of the server side transport.
		 * @param sctpParameters - SCTP parameters of the server side transport.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 * @param appData - Custom data to attach to the transport.
		 *
		 * @remarks
		 * - The returned transport is owned by the application, which must close and
		 *   destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 * @throws MediaSoupClientTypeError if appData is not a JSON object.
		 */
		[[nodiscard]] RecvTransport* CreateRecvTransport(
		  RecvTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;

		/**
		 * Creates a Transport for receiving media.
		 *
		 * @param listener - Listener of the transport events.
		 * @param id - Transport id given by the mediasoup Router.
		 * @param iceParameters - ICE parameters of the server side transport.
		 * @param iceCandidates - ICE candidates of the server side transport.
		 * @param dtlsParameters - DTLS parameters of the server side transport.
		 * @param peerConnectionOptions - Options for the underlying PeerConnection.
		 * @param appData - Custom data to attach to the transport.
		 *
		 * @remarks
		 * - The returned transport is owned by the application, which must close and
		 *   destroy it.
		 *
		 * @throws MediaSoupClientInvalidStateError if not loaded.
		 * @throws MediaSoupClientTypeError if appData is not a JSON object.
		 */
		[[nodiscard]] RecvTransport* CreateRecvTransport(
		  RecvTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;

	private:
		// Loaded flag.
		bool loaded{ false };
		// Extended RTP capabilities.
		std::function<nlohmann::json(nlohmann::json&)> getSendExtendedRtpCapabilities;
		// Local RTP capabilities for receiving media.
		nlohmann::json recvRtpCapabilities;
		// Whether we can produce audio/video based on computed extended RTP capabilities.
		// clang-format off
		std::map<std::string, bool> canProduceByKind =
		{
			{ "audio", false },
			{ "video", false }
		};
		// clang-format on
	};
} // namespace mediasoupclient

#endif
