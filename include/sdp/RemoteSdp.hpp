#ifndef MSC_REMOTESDP_HPP
#define MSC_REMOTESDP_HPP

#include "json.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
namespace Sdp
{
	class RemoteSdp
	{
	public:
		explicit RemoteSdp(json transportRemoteParameters, json sendingRtpParametersByKind = json::array());

	public:
		std::string CreateAnswerSdp(const json& localSdpObj);
		std::string CreateOfferSdp(const json& receiverInfos);
		void UpdateTransportRemoteIceParameters(const json& remoteIceParameters);

	protected:
		// Generic sending RTP parameters for audio and video.
		json rtpParametersByKind = json::object();

		// Transport remote parameters, including ICE parameters, ICE candidates
		// and DTLS parameteres.
		json transportRemoteParameters = json::object();

		// Generic sending RTP parameters for audio and video.
		json sendingRtpParametersByKind = json::object();

		// SDP global fields.
		json sdpGlobalFields = json::object();
	};
} // namespace Sdp
} // namespace mediasoupclient

#endif
