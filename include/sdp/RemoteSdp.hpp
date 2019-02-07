#ifndef MSC_REMOTESDP_HPP
#define MSC_REMOTESDP_HPP

#include "json.hpp"

namespace mediasoupclient
{
namespace Sdp
{
	class RemoteSdp
	{
	public:
		RemoteSdp(
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters,
		  nlohmann::json sendingRtpParametersByKind = nlohmann::json::array());

	public:
		std::string CreateAnswerSdp(const nlohmann::json& localSdpObj);
		std::string CreateOfferSdp(const nlohmann::json& receiverInfos);
		void UpdateTransportRemoteIceParameters(const nlohmann::json& remoteIceParameters);
		void UpdateTransportRemoteDtlsRole(const std::string& remoteRole);

	protected:
		// Generic sending RTP parameters for audio and video.
		nlohmann::json rtpParametersByKind = nlohmann::json::object();

		// Transport remote parameters, including ICE parameters, ICE candidates
		// and DTLS parameteres.
		nlohmann::json remoteIceParameters  = nlohmann::json::object();
		nlohmann::json remoteIceCandidates  = nlohmann::json::object();
		nlohmann::json remoteDtlsParameters = nlohmann::json::object();

		// Generic sending RTP parameters for audio and video.
		nlohmann::json sendingRtpParametersByKind = nlohmann::json::object();

		// SDP global fields.
		nlohmann::json sdpGlobalFields = nlohmann::json::object();
	};

	inline void RemoteSdp::UpdateTransportRemoteDtlsRole(const std::string& remoteRole)
	{
		this->remoteDtlsParameters["role"] = remoteRole;
	}
} // namespace Sdp
} // namespace mediasoupclient

#endif
