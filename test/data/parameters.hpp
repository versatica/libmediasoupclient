#ifndef MSC_TEST_PARAMETERS
#define MSC_TEST_PARAMETERS

#include "json.hpp"

using json = nlohmann::json;

json generateRouterRtpCapabilities();
json generateRtpParametersByKind();
json generateLocalDtlsParameters();
json generateTransportRemoteParameters();
std::string generateProducerRemoteId();
json generateConsumerRemoteParameters(const std::string& codecMimeType);

#endif
