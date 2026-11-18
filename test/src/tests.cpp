#define CATCH_CONFIG_RUNNER

#include "Logger.hpp"
// NOTE: Included for the libwebrtc declarations it re-exports (such as
// webrtc::LoggingSeverity), not used directly.
#include "PeerConnection.hpp" // IWYU pragma: keep
#include "mediasoupclient.hpp"
#include <catch.hpp>
#include <string>

// NOTE: This is the test runner entry point, Catch2 already reports any
// exception thrown by the test cases themselves.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[])
{
	mediasoupclient::Logger::LogLevel logLevel{ mediasoupclient::Logger::LogLevel::LOG_NONE };
	webrtc::LoggingSeverity webrtcLogLevel{ webrtc::LoggingSeverity::LS_NONE };
	Catch::Session session;
	std::string logLevelStr;
	std::string webrtcLogLevelStr;
	int ret;

	// Build a new parser on top of Catch.
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
	// clang-format off
	auto const cli = session.cli()
	  | Catch::clara::Opt(logLevelStr, "debug|warn|error|none")["-L"]["--log-level"]("libmediasoupclient log level (default: none)")
	  | Catch::clara::Opt(webrtcLogLevelStr, "verbose|info|warn|error|none")["-W"]["--webrtc-log-level"]("libwebrtc log level (default: none)");
	// clang-format on
	// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

	// Now pass the new composite back to Catch so it uses that.
	session.cli(cli);

	// Let Catch (using Clara) parse the command line.
	ret = session.applyCommandLine(argc, argv);

	if (ret != 0) // Indicates a command line error.
	{
		return ret;
	}

	// Apply log levels.
	if (logLevelStr == "debug")
	{
		logLevel = mediasoupclient::Logger::LogLevel::LOG_DEBUG;
	}
	else if (logLevelStr == "warn")
	{
		logLevel = mediasoupclient::Logger::LogLevel::LOG_WARN;
	}
	else if (logLevelStr == "error")
	{
		logLevel = mediasoupclient::Logger::LogLevel::LOG_ERROR;
	}

	if (webrtcLogLevelStr == "verbose")
	{
		webrtcLogLevel = webrtc::LoggingSeverity::LS_VERBOSE;
	}
	else if (webrtcLogLevelStr == "info")
	{
		webrtcLogLevel = webrtc::LoggingSeverity::LS_INFO;
	}
	else if (webrtcLogLevelStr == "warn")
	{
		webrtcLogLevel = webrtc::LoggingSeverity::LS_WARNING;
	}
	else if (webrtcLogLevelStr == "error")
	{
		webrtcLogLevel = webrtc::LoggingSeverity::LS_ERROR;
	}

	mediasoupclient::Logger::SetLogLevel(logLevel);
	mediasoupclient::Logger::SetDefaultHandler();

	webrtc::LogMessage::LogToDebug(webrtcLogLevel);

	// Initialization.
	mediasoupclient::Initialize();

	ret = session.run(argc, argv);

	// Cleanup.
	mediasoupclient::Cleanup();

	return ret;
}
