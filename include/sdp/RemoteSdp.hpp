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
		explicit RemoteSdp(
		  nlohmann::json transportRemoteParameters,
		  nlohmann::json sendingRtpParametersByKind = nlohmann::json::array());

	public:
		std::string CreateAnswerSdp(const nlohmann::json& localSdpObj);
		std::string CreateOfferSdp(const nlohmann::json& receiverInfos);
		void UpdateTransportRemoteIceParameters(const nlohmann::json& remoteIceParameters);

	protected:
		// Generic sending RTP parameters for audio and video.
		nlohmann::json rtpParametersByKind = nlohmann::json::object();

		// Transport remote parameters, including ICE parameters, ICE candidates
		// and DTLS parameteres.
		nlohmann::json transportRemoteParameters = nlohmann::json::object();

		// Generic sending RTP parameters for audio and video.
		nlohmann::json sendingRtpParametersByKind = nlohmann::json::object();

		// SDP global fields.
		nlohmann::json sdpGlobalFields = nlohmann::json::object();
	};
} // namespace Sdp
} // namespace mediasoupclient

#endif
