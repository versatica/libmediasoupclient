#define MSC_CLASS "mediasoupclient"

#include "mediasoupclient.hpp"
#include "version.hpp"
#include <rtc_base/ssl_adapter.h>
#include <sstream>

namespace mediasoupclient
{
	void Initialize()
	{
		MSC_TRACE();

		MSC_DEBUG("mediasoupclient v%s", Version().c_str());

		webrtc::InitializeSSL();
	}

	void Cleanup()
	{
		MSC_TRACE();

		webrtc::CleanupSSL();
	}

	std::string Version()
	{
		MSC_TRACE();

		std::stringstream ss;

		ss << MEDIASOUPCLIENT_VERSION_MAJOR << "." << MEDIASOUPCLIENT_VERSION_MINOR << "."
		   << MEDIASOUPCLIENT_VERSION_PATCH;

		return ss.str();
	}
} // namespace mediasoupclient
