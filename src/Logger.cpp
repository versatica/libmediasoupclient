#define MSC_CLASS "Logger"

#include "Logger.hpp"
#include <iostream>
#include <memory> // std::addressof()

namespace mediasoupclient
{
	/* Class variables. */

	Logger::LogHandlerInterface* Logger::handler{ nullptr };
	thread_local char Logger::buffer[Logger::BufferSize];
	Logger::LogLevel Logger::logLevel = Logger::LogLevel::LOG_NONE;

	/* Class methods. */

	void Logger::SetLogLevel(Logger::LogLevel level)
	{
		Logger::logLevel = level;
	}

	void Logger::SetHandler(LogHandlerInterface* handler)
	{
		Logger::handler = handler;
	}

	void Logger::SetDefaultHandler()
	{
		// NOTE: Static so that it outlives any logging done by the application, and
		// so that setting the default handler twice does not leak the first one.
		static Logger::DefaultLogHandler defaultLogHandler;

		Logger::handler = std::addressof(defaultLogHandler);
	}

	/* DefaultLogHandler */

	void Logger::DefaultLogHandler::OnLog(LogLevel /*level*/, char* payload, size_t /*len*/)
	{
		std::cout << payload << std::endl;
	}
} // namespace mediasoupclient
