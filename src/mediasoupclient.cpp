#define MSC_CLASS "mediasoupclient"

#include "mediasoupclient.hpp"
#include "Logger.hpp"
#include "version.hpp"
#include <rtc_base/ssl_adapter.h>
#include <sstream>

namespace mediasoupclient
{
	void Initialize() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		MSC_DEBUG("mediasoupclient v%s", Version().c_str());

		webrtc::InitializeSSL();
	}

	void Cleanup() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		webrtc::CleanupSSL();
	}

	std::string Version() // NOLINT(readability-identifier-naming)
	{
		std::stringstream ss;

		ss << MEDIASOUPCLIENT_VERSION_MAJOR << "." << MEDIASOUPCLIENT_VERSION_MINOR << "."
		   << MEDIASOUPCLIENT_VERSION_PATCH;

		return ss.str();
	}
} // namespace mediasoupclient
