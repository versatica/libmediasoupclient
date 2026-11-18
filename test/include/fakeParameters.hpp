#ifndef MSC_TEST_FAKE_PARAMETERS
#define MSC_TEST_FAKE_PARAMETERS

#include <json.hpp>

// NOTE: An alias and not a using declaration, which would bring the name into
// the global namespace of every file including this header.
// NOLINTNEXTLINE(readability-redundant-qualified-alias)
using json = nlohmann::json;

json generateRouterRtpCapabilities();

json generateRtpParametersByKind();

json generateLocalDtlsParameters();

json generateTransportRemoteParameters();

std::string generateProducerRemoteId();

json generateConsumerRemoteParameters(const std::string& codecMimeType);

#endif
