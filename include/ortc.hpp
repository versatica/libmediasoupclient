#ifndef MSC_ORTC_HPP
#define MSC_ORTC_HPP

#include "json.hpp"
#include <string>

namespace mediasoupclient
{
namespace ortc
{
	nlohmann::json getExtendedRtpCapabilities(nlohmann::json& localCaps, const nlohmann::json& remoteCaps);
	nlohmann::json getRecvRtpCapabilities(const nlohmann::json& extendedRtpCapabilities);
	nlohmann::json getSendingRtpParameters(
	  const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
	nlohmann::json getSendingRemoteRtpParameters(
	  const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
	bool canSend(const std::string& kind, const nlohmann::json& extendedRtpCapabilities);
	bool canReceive(const nlohmann::json& rtpParameters, const nlohmann::json& extendedRtpCapabilities);
} // namespace ortc
} // namespace mediasoupclient

#endif
