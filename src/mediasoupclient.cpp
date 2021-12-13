#define MSC_CLASS "mediasoupclient"

#include "mediasoupclient.hpp"
#include "Logger.hpp"
#include "version.hpp"
#include <rtc_base/helpers.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/time_utils.h>
#include <sstream>
#include <system_wrappers/include/field_trial.h>

namespace mediasoupclient
{
	void Initialize() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		MSC_DEBUG("mediasoupclient v%s", Version().c_str());

		webrtc::field_trial::InitFieldTrialsFromString("WebRTC-SupportVP9SVC/EnabledByFlag_3SL3TL/");

		rtc::InitializeSSL();
		rtc::InitRandom(rtc::Time());
	}

	void Cleanup() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		rtc::CleanupSSL();
	}

	std::string Version() // NOLINT(readability-identifier-naming)
	{
		std::stringstream ss;

		ss << MEDIASOUPCLIENT_VERSION_MAJOR << "." << MEDIASOUPCLIENT_VERSION_MINOR << "."
		   << MEDIASOUPCLIENT_VERSION_PATCH;

		return ss.str();
	}
} // namespace mediasoupclient
