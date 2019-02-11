#ifndef MSC_SDP_UTILS_HPP
#define MSC_SDP_UTILS_HPP

#include "json.hpp"
#include "sdptransform.hpp"
#include "api/media_stream_interface.h"

namespace mediasoupclient
{
namespace Sdp
{
	namespace Utils
	{
		json extractRtpCapabilities(const json& sdpObj);
		json extractDtlsParameters(const json& sdpObj);
		void fillRtpParametersForTrack(json& rtpParameters, const json& sdpObj, const std::string& mid);
		void addLegacySimulcast(json& offerMediaObject, uint8_t numStreams);
		std::string getCname(const json& offerMediaObject);
		json getRtpEncodings(const json& offerMediaObject);
		void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject);
	} // namespace Utils
} // namespace Sdp
} // namespace mediasoupclient

#endif
