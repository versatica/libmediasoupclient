#define MSC_CLASS "mediasoupclient"

#include "mediasoupclient.hpp"
#include "Logger.hpp"
#include "version.hpp"
#include <rtc_base/helpers.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/log_sinks.h>
#include <sstream>

const size_t kDefaultMaxLogFileSize = 10 * 1024 * 1024;
static std::unique_ptr<rtc::FileRotatingLogSink> log_sink;

namespace mediasoupclient
{
	void Initialize() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		MSC_DEBUG("mediasoupclient v%s", Version().c_str());

		rtc::InitializeSSL();
		rtc::InitRandom(rtc::Time());
		
		//Init debug log
		int debug_log_level = rtc::LS_VERBOSE;
    		rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)debug_log_level);
    		rtc::LogMessage::LogTimestamps();
    		rtc::LogMessage::LogThreads();
    
		//Init file log sink
    		log_sink.reset(new rtc::FileRotatingLogSink("./", "webrtc_logs", kDefaultMaxLogFileSize, 10));
	    	if (log_sink->Init() == false) {
	        	RTC_LOG(LS_ERROR) << __FUNCTION__ << "Failed to open webrtc log file";
	        	log_sink.reset();
	    	}else {
			rtc::LogMessage::AddLogToStream(log_sink.get(), rtc::LS_VERBOSE);
		}
	    	
	}

	void Cleanup() // NOLINT(readability-identifier-naming)
	{
		MSC_TRACE();

		rtc::CleanupSSL();
		
		log_sink.reset();
	}

	std::string Version() // NOLINT(readability-identifier-naming)
	{
		std::stringstream ss;

		ss << MEDIASOUPCLIENT_VERSION_MAJOR << "." << MEDIASOUPCLIENT_VERSION_MINOR << "."
		   << MEDIASOUPCLIENT_VERSION_PATCH;

		return ss.str();
	}
} // namespace mediasoupclient
