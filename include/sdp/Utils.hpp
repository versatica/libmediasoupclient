#ifndef MSC_SDP_UTILS_HPP
#define MSC_SDP_UTILS_HPP

#include "json.hpp"
#include "sdptransform.hpp"
#include "api/mediastreaminterface.h"

namespace mediasoupclient
{
namespace Sdp
{
	namespace Utils
	{
		json extractRtpCapabilities(const json& sdpObj);
		json extractDtlsParameters(const json& sdpObj);
		void fillRtpParametersForTrack(json& rtpParameters, const json& sdpObj, const std::string& mid);
		void addLegacySimulcast(
		  json& sdpObj, const webrtc::MediaStreamTrackInterface* track, uint8_t numStreams);
	} // namespace Utils
} // namespace Sdp
} // namespace mediasoupclient

#endif
