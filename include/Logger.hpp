/**
 * Logger facility.
 *
 * This include file defines logging macros for source files (.cpp). Each
 * source file including Logger.hpp MUST define its own MSC_CLASS macro. Include
 * files (.h) MUST NOT include Logger.hpp.
 *
 * All the logging macros use the same format as printf(). The XXX version
 * of a macro logs to stdoud/stderr instead of using the Channel instance.
 *
 * If the macro MSC_LOG_FILE_LINE is defied, all the logging macros print more
 * verbose information, including current file and line.
 *
 * MSC_TRACE()
 *
 *   Logs the current method/function if MSC_LOG_TRACE macro is defined and the
 *   current debug level is "debug".
 *
 * MSC_DEBUG(...)
 *
 * 	 Logs if the current source file defines the MSC_LOG_DEV macro.
 *
 * MSC_WARN(...)
 *
 *   Logs a warning.
 *
 * MSC_ERROR(...)
 *
 *   Logs an error. Must just be used for internal errors that should not
 *   happen.
 *
 * MSC_ABORT(...)
 *
 *   Logs the given error to stderr and aborts the process.
 *
 * MSC_ASSERT(condition, ...)
 *
 *   If the condition is not satisfied, it calls MSC_ABORT().
 */

#ifndef MSC_LOGGER_HPP
#define MSC_LOGGER_HPP

#include <cstdio>  // std::snprintf(), std::fprintf(), stdout, stderr
#include <cstdlib> // std::abort()
#include <cstring>
#include <string>

// clang-format off

#ifdef MSC_LOG_DEV
	#define _MSC_LOG_DEV_ENABLED true
#else
	#define _MSC_LOG_DEV_ENABLED false
#endif

namespace mediasoupclient
{

class Logger
{
public:
	enum class LogLevel : uint8_t
	{
		LOG_NONE  = 0,
		LOG_ERROR = 1,
		LOG_WARN  = 2,
		LOG_DEBUG = 3,
		LOG_TRACE = 4,
	};

	class LogHandlerInterface
	{
	public:
		virtual void OnLog(LogLevel level, char* payload, size_t len) = 0;
	};

	class DefaultLogHandler : public LogHandlerInterface
	{
		void OnLog(LogLevel level, char* payload, size_t len) override;
	};

	static void SetLogLevel(LogLevel level);
	static void SetHandler(LogHandlerInterface* handler);
	static void SetDefaultHandler();

public:
	static LogLevel logLevel;
	static LogHandlerInterface* handler;
	static const size_t bufferSize {10000};
	static char buffer[];
};

/* Logging macros. */

#define _MSC_LOG_SEPARATOR_CHAR "\n"

#ifdef MSC_LOG_FILE_LINE
	#define _MSC_LOG_STR " %s:%d | %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "
	#define _MSC_FILE (std::strchr(__FILE__, '/') ? std::strchr(__FILE__, '/') + 1 : __FILE__)
	#define _MSC_LOG_ARG _MSC_FILE, __LINE__, MSC_CLASS, __FUNCTION__
#else
	#define _MSC_LOG_STR " %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "
	#define _MSC_LOG_ARG MSC_CLASS, __FUNCTION__
#endif

#ifdef MSC_LOG_TRACE
	#define MSC_TRACE() \
		do \
		{ \
			if (Logger::handler) \
			{ \
				if (Logger::logLevel == Logger::LogLevel::LOG_DEBUG) \
				{ \
					int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[TRACE]" _MSC_LOG_STR, _MSC_LOG_ARG); \
						Logger::handler->OnLog(Logger::LogLevel::LOG_TRACE, Logger::buffer, loggerWritten); \
				} \
			} \
		} \
		while (false)
#else
	#define MSC_TRACE() ;
#endif

#ifdef MSC_LOG_DEV
	#define MSC_DEBUG(desc, ...) \
		do \
		{ \
			if (Logger::handler) \
			{ \
				if (Logger::logLevel == Logger::LogLevel::LOG_DEBUG) \
				{ \
					int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[DEBUG]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
						Logger::handler->OnLog(Logger::LogLevel::LOG_DEBUG, Logger::buffer, loggerWritten); \
				} \
			} \
		} \
		while (false)
#else
	#define MSC_DEBUG(desc, ...) ;
#endif

#define MSC_WARN(desc, ...) \
	do \
	{ \
		if (Logger::handler) \
		{ \
			if (Logger::logLevel >= Logger::LogLevel::LOG_WARN) \
			{ \
				int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[WARN]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
					Logger::handler->OnLog(Logger::LogLevel::LOG_WARN, Logger::buffer, loggerWritten); \
			} \
		} \
	} \
	while (false)

#define MSC_ERROR(desc, ...) \
	do \
	{ \
		if (Logger::handler) \
		{ \
			int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[ERROR]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			Logger::handler->OnLog(Logger::LogLevel::LOG_ERROR, Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_ABORT(desc, ...) \
	do \
	{ \
		std::fprintf(stderr, "ABORT" _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, _MSC_LOG_ARG, ##__VA_ARGS__); \
		std::fflush(stderr); \
		std::abort(); \
	} \
	while (false)

#define MSC_ASSERT(condition, desc, ...) \
	if (!(condition)) \
	{ \
		MSC_ABORT("failed assertion `%s': " desc, #condition, ##__VA_ARGS__); \
	}

} // namespace mediasoupclient

// clang-format on

#endif
