#ifndef MSC_ORTC_HPP
#define MSC_ORTC_HPP

#include "json.hpp"
#include <string>

using json = nlohmann::json;

namespace mediasoupclient
{
namespace ortc
{
	json getExtendedCapabilities(const json& localCaps, const json& remoteCaps);
	json getRecvRtpCapabilities(const json& extendedRtpCapabilities);
	json getSendingRtpParameters(const std::string& kind, const json& extendedRtpCapabilities);
	bool canSend(const std::string& kind, const json& extendedRtpCapabilities);
	bool canReceive(const json& rtpParameters, const json& extendedRtpCapabilities);
} // namespace ortc
} // namespace mediasoupclient

#endif
