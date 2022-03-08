#define MSC_CLASS "Logger"

#include "Logger.hpp"
#include <iostream>

namespace mediasoupclient
{
	/* Class variables. */

	Logger::LogHandlerInterface* Logger::handler{ nullptr };
	char Logger::buffer[Logger::bufferSize];
	Logger::LogLevel Logger::logLevel = Logger::LogLevel::LOG_NONE;
	std::ofstream Logger::stream;

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
		Logger::handler = new Logger::DefaultLogHandler();
	}

	void Logger::setLogPath(const char* pFilePath)
	{
		if (!stream.is_open())
			stream.open(pFilePath, std::ios_base::binary);
	}

	/* DefaultLogHandler */

	void Logger::DefaultLogHandler::OnLog(LogLevel /*level*/, char* payload, size_t /*len*/)
	{
		std::cout << payload << std::endl;
		stream << payload << std::endl;
	}
} // namespace mediasoupclient
