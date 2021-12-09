#ifndef MSC_ORTC_HPP
#define MSC_ORTC_HPP

#include <json.hpp>
#include <string>

namespace mediasoupclient
{
	namespace ortc
	{
		void validateRtpCapabilities(nlohmann::json& caps);
		void validateRtpCodecCapability(nlohmann::json& codec);
		void validateRtcpFeedback(nlohmann::json& fb);
		void validateRtpHeaderExtension(nlohmann::json& ext);
		void validateRtpParameters(nlohmann::json& params);
		void validateRtpCodecParameters(nlohmann::json& codec);
		void validateRtpHeaderExtensionParameters(nlohmann::json& ext);
		void validateRtpEncodingParameters(nlohmann::json& encoding);
		void validateRtcpParameters(nlohmann::json& rtcp);
		void validateSctpCapabilities(nlohmann::json& caps);
		void validateNumSctpStreams(nlohmann::json& numStreams);
		void validateSctpParameters(nlohmann::json& params);
		void validateSctpStreamParameters(nlohmann::json& params);
		void validateIceParameters(nlohmann::json& params);
		void validateIceCandidates(nlohmann::json& params);
		void validateDtlsParameters(nlohmann::json& params);
		void validateProducerCodecOptions(nlohmann::json& params);
		nlohmann::json getExtendedRtpCapabilities(nlohmann::json& localCaps, nlohmann::json& remoteCaps);
		nlohmann::json getRecvRtpCapabilities(const nlohmann::json& extendedRtpCapabilities);
		nlohmann::json getSendingRtpParameters(
		  const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
		nlohmann::json getSendingRemoteRtpParameters(
		  const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
		const nlohmann::json generateProbatorRtpParameters(const nlohmann::json& videoRtpParameters);
		bool canSend(const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
		bool canReceive(nlohmann::json& rtpParameters, const nlohmann::json& extendedRtpCapabilities);
		nlohmann::json reduceCodecs(nlohmann::json& codecs, const nlohmann::json* capCodec = nullptr);
	} // namespace ortc
} // namespace mediasoupclient

#endif
