/**
 * Logger facility.
 *
 * This include file defines logging macros for source files (.cpp). Each
 * source file including Logger.hpp MUST define its own MSC_CLASS macro. Include
 * files (.hpp) MUST NOT include Logger.hpp.
 *
 * All the logging macros use the same format as printf().
 *
 * If the macro MSC_LOG_FILE_LINE is defied, all the logging macros print more
 * verbose information, including current file and line.
 *
 * MSC_TRACE()
 *
 *   Logs the current method/function if MSC_LOG_TRACE macro is defined and
 *   logLevel is 'debug'.
 *
 * MSC_DEBUG(...)
 *
 * 	 Logs a debug.
 *
 * MSC_WARN(...)
 *
 *   Logs a warning.
 *
 * MSC_ERROR(...)
 *
 *   Logs an error.
 *
 * MSC_DUMP(...)
 *
 * 	 Logs always to stdout.
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

#include <cstdint> // uint8_t
#include <cstdio>  // std::snprintf(), std::fprintf(), stdout, stderr
#include <cstdlib> // std::abort()
#include <cstring>

namespace mediasoupclient
{
	class Logger
	{
	public:
		static constexpr size_t BufferSize{ 50000 };

	public:
		enum class LogLevel : uint8_t
		{
			LOG_NONE  = 0,
			LOG_ERROR = 1,
			LOG_WARN  = 2,
			LOG_DEBUG = 3,
			LOG_TRACE = 4
		};

		class LogHandlerInterface
		{
		public:
			virtual ~LogHandlerInterface() = default;

			virtual void OnLog(LogLevel level, char* payload, size_t len) = 0;
		};

		class DefaultLogHandler : public LogHandlerInterface
		{
		public:
			void OnLog(LogLevel level, char* payload, size_t len) override;
		};

	public:
		static void SetLogLevel(LogLevel level);

		static void SetHandler(LogHandlerInterface* handler);

		static void SetDefaultHandler();

	public:
		static LogLevel logLevel;
		static LogHandlerInterface* handler;
		// NOTE: A buffer per thread to avoid corrupted logs.
		static thread_local char buffer[];
	};
} // namespace mediasoupclient

// clang-format off

/* Logging macros. */

// NOLINTBEGIN
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
// NOLINTEND

#ifdef MSC_LOG_TRACE
	#define MSC_TRACE() \
		do \
		{ \
			if (mediasoupclient::Logger::handler && mediasoupclient::Logger::logLevel == mediasoupclient::Logger::LogLevel::LOG_DEBUG) \
			{ \
				const int loggerWritten = std::snprintf(mediasoupclient::Logger::buffer, mediasoupclient::Logger::BufferSize, "[TRACE]" _MSC_LOG_STR, _MSC_LOG_ARG); \
				mediasoupclient::Logger::handler->OnLog(mediasoupclient::Logger::LogLevel::LOG_TRACE, mediasoupclient::Logger::buffer, loggerWritten); \
			} \
		} \
		while (false)
#else
	#define MSC_TRACE() ;
#endif

#define MSC_DEBUG(desc, ...) \
	do \
	{ \
		if (mediasoupclient::Logger::handler && mediasoupclient::Logger::logLevel == mediasoupclient::Logger::LogLevel::LOG_DEBUG) \
		{ \
			const int loggerWritten = std::snprintf(mediasoupclient::Logger::buffer, mediasoupclient::Logger::BufferSize, "[DEBUG]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			mediasoupclient::Logger::handler->OnLog(mediasoupclient::Logger::LogLevel::LOG_DEBUG, mediasoupclient::Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_WARN(desc, ...) \
	do \
	{ \
		if (mediasoupclient::Logger::handler && mediasoupclient::Logger::logLevel >= mediasoupclient::Logger::LogLevel::LOG_WARN) \
		{ \
			const int loggerWritten = std::snprintf(mediasoupclient::Logger::buffer, mediasoupclient::Logger::BufferSize, "[WARN]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			mediasoupclient::Logger::handler->OnLog(mediasoupclient::Logger::LogLevel::LOG_WARN, mediasoupclient::Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_ERROR(desc, ...) \
	do \
	{ \
		if (mediasoupclient::Logger::handler && mediasoupclient::Logger::logLevel >= mediasoupclient::Logger::LogLevel::LOG_ERROR) \
		{ \
			const int loggerWritten = std::snprintf(mediasoupclient::Logger::buffer, mediasoupclient::Logger::BufferSize, "[ERROR]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			mediasoupclient::Logger::handler->OnLog(mediasoupclient::Logger::LogLevel::LOG_ERROR, mediasoupclient::Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_DUMP(desc, ...) \
	do \
	{ \
		std::fprintf(stdout, _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, _MSC_LOG_ARG, ##__VA_ARGS__); \
		std::fflush(stdout); \
	} \
	while (false)

#define MSC_ABORT(desc, ...) \
	do \
	{ \
		std::fprintf(stderr, "[ABORT]" _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, _MSC_LOG_ARG, ##__VA_ARGS__); \
		std::fflush(stderr); \
		std::abort(); \
	} \
	while (false)

#define MSC_ASSERT(condition, desc, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			MSC_ABORT("failed assertion `%s': " desc, #condition, ##__VA_ARGS__); \
		} \
	} \
	while (false)

// clang-format on

#endif
