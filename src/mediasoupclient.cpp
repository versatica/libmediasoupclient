#define MSC_CLASS "mediasoupclient"
#define MSC_LOG_DEV

#include "Logger.hpp"
#include "version.hpp"
#include "rtc_base/helpers.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/time_utils.h"

namespace mediasoupclient
{
void Initialize()
{
	MSC_TRACE();

	MSC_DEBUG(
	  "mediasoupclient v%d.%d.%d",
	  MEDIASOUPCLIENT_VERSION_MAJOR,
	  MEDIASOUPCLIENT_VERSION_MINOR,
	  MEDIASOUPCLIENT_VERSION_PATCH);

	rtc::InitializeSSL();
	rtc::InitRandom(rtc::Time());
}

void Cleanup()
{
	MSC_TRACE();

	rtc::CleanupSSL();
}
} // namespace mediasoupclient
