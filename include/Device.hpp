#ifndef MSC_DEVICE_HPP
#define MSC_DEVICE_HPP

#include "Handler.hpp"
#include "Transport.hpp"
#include <json.hpp>
#include <map>
#include <string>

namespace mediasoupclient
{
	class Device
	{
	public:
		Device()  = default;
		~Device() = default;

		bool IsLoaded() const;
		const nlohmann::json& GetRtpCapabilities() const;
		const nlohmann::json& GetSctpCapabilities() const;
		void Load(
		  nlohmann::json routerRtpCapabilities,
		  const PeerConnection::Options* peerConnectionOptions = nullptr);
		bool CanProduce(const std::string& kind);
		SendTransport* CreateSendTransport(
		  SendTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;
		SendTransport* CreateSendTransport(
		  SendTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;
		RecvTransport* CreateRecvTransport(
		  RecvTransport::Listener* listener,
		  const std::string& id,
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  const nlohmann::json& sctpParameters,
		  const PeerConnection::Options* peerConnectionOptions = nullptr,
		  const nlohmann::json& appData                        = nlohmann::json::object()) const;
		RecvTransport* CreateRecvTransport(
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
		nlohmann::json extendedRtpCapabilities;
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
		// Local SCTP capabilities.
		nlohmann::json sctpCapabilities;
	};
} // namespace mediasoupclient

#endif
