#define CATCH_CONFIG_RUNNER

#include "include/catch.hpp"
#include "mediasoupclient.hpp"
#include "PeerConnection.hpp"
#include "Logger.hpp"

using namespace mediasoupclient;

int main(int argc, char* argv[])
{
	// Set RTC logging severity to warning.
	// rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_WARNING);

	// There must be exactly one instance.
	Catch::Session session;

	int logLevel = 0;

	// Build a new parser on top of Catch's.
	using namespace Catch::clara;
	auto cli
		= session.cli()               // Get Catch's composite command line parser.
		| Opt( logLevel, "logLevel" ) // Bind variable to a new option.
		["-L"]["--log-level"]         // The option names it will respond to.
		("log level");                // Description string for the help output.

	// Now pass the new composite back to Catch so it uses that.
	session.cli(cli);

	// Let Catch (using Clara) parse the command line.
	int returnCode = session.applyCommandLine( argc, argv );
	if( returnCode != 0 ) // Indicates a command line error.
		return returnCode;

	if (logLevel > 0)
	{
		auto level = static_cast<Logger::LogLevel>(logLevel);

		Logger::SetLogLevel(level);
		Logger::SetHandler(new Logger::DefaultLogHandler());
	}

	// Initialization.
	mediasoupclient::Initialize();

	int ret = session.run(argc, argv);

	// Cleanup.
	mediasoupclient::Cleanup();

	return ret;
}
