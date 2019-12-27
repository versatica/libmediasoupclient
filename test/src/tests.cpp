#define CATCH_CONFIG_RUNNER

#include "Logger.hpp"
#include "PeerConnection.hpp"
#include "mediasoupclient.hpp"
#include <catch.hpp>
#include <string>

int main(int argc, char* argv[])
{
	mediasoupclient::Logger::LogLevel logLevel{ mediasoupclient::Logger::LogLevel::LOG_NONE };
	rtc::LoggingSeverity webrtcLogLevel{ rtc::LoggingSeverity::LS_NONE };
	Catch::Session session;
	std::string logLevelStr;
	std::string webrtcLogLevelStr;
	int ret;

	// Build a new parser on top of Catch.
	// clang-format off
	auto cli = session.cli()
	  | Catch::clara::Opt(logLevelStr, "debug|warn|error|none")["-L"]["--log-level"]("libmediasoupclient log level (default: none)")
	  | Catch::clara::Opt(webrtcLogLevelStr, "verbose|info|warn|error|none")["-W"]["--webrtc-log-level"]("libwebrtc log level (default: none)");
	// clang-format on

	// Now pass the new composite back to Catch so it uses that.
	session.cli(cli);

	// Let Catch (using Clara) parse the command line.
	ret = session.applyCommandLine(argc, argv);

	if (ret != 0) // Indicates a command line error.
		return ret;

	// Apply log levels.
	if (logLevelStr == "debug")
		logLevel = mediasoupclient::Logger::LogLevel::LOG_DEBUG;
	else if (logLevelStr == "warn")
		logLevel = mediasoupclient::Logger::LogLevel::LOG_WARN;
	else if (logLevelStr == "error")
		logLevel = mediasoupclient::Logger::LogLevel::LOG_ERROR;

	if (webrtcLogLevelStr == "verbose")
		webrtcLogLevel = rtc::LoggingSeverity::LS_VERBOSE;
	else if (webrtcLogLevelStr == "info")
		webrtcLogLevel = rtc::LoggingSeverity::LS_INFO;
	else if (webrtcLogLevelStr == "warn")
		webrtcLogLevel = rtc::LoggingSeverity::LS_WARNING;
	else if (webrtcLogLevelStr == "error")
		webrtcLogLevel = rtc::LoggingSeverity::LS_ERROR;

	mediasoupclient::Logger::SetLogLevel(logLevel);
	mediasoupclient::Logger::SetHandler(new mediasoupclient::Logger::DefaultLogHandler());

	rtc::LogMessage::LogToDebug(webrtcLogLevel);

	// Initialization.
	mediasoupclient::Initialize();

	ret = session.run(argc, argv);

	// Cleanup.
	mediasoupclient::Cleanup();

	return ret;
}
