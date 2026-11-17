/**
 * Logger facility.
 *
 * This include file defines logging macros for source files (.cpp). Each
 * source file including Logger.hpp MUST define its own MSC_CLASS macro.
 * Include files (.hpp) MUST NOT include Logger.hpp.
 *
 * All logging macros use the same format as printf().
 *
 * If MSC_LOG_FILE_LINE is defined, logging macros include the current
 * file and line.
 */

#ifndef MSC_LOGGER_HPP
#define MSC_LOGGER_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
			LOG_TRACE = 4
		};

		class LogHandlerInterface
		{
		public:
			virtual ~LogHandlerInterface() = default;

			virtual void OnLog(
			  LogLevel level,
			  char* payload,
			  size_t len) = 0;
		};

		class DefaultLogHandler : public LogHandlerInterface
		{
			void OnLog(
			  LogLevel level,
			  char* payload,
			  size_t len) override;
		};

		static void SetLogLevel(LogLevel level);
		static void SetHandler(LogHandlerInterface* handler);
		static void SetDefaultHandler();

	public:
		static LogLevel logLevel;
		static LogHandlerInterface* handler;
		static const size_t BufferSize{ 50000 };
		static char buffer[];
	};
} // namespace mediasoupclient

// clang-format off

/* Logging macros. */

// NOLINTBEGIN

#define _MSC_LOG_SEPARATOR_CHAR "\n"

#ifdef MSC_LOG_FILE_LINE

	#define _MSC_LOG_STR " %s:%d | %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "

	#define _MSC_FILE \
		(std::strchr(__FILE__, '/') \
			? std::strchr(__FILE__, '/') + 1 \
			: __FILE__)

	#define _MSC_LOG_ARG \
		_MSC_FILE, __LINE__, MSC_CLASS, __FUNCTION__

#else

	#define _MSC_LOG_STR " %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "

	#define _MSC_LOG_ARG \
		MSC_CLASS, __FUNCTION__

#endif

/*
 * Safely format and dispatch a log message.
 *
 * The returned length from snprintf() is clamped to the actual
 * buffer capacity to prevent out-of-bounds reads.
 *
 * NOTE:
 * The shared static buffer is retained for compatibility with
 * the existing Logger.cpp implementation.
 * Concurrent logging still requires synchronization.
 */

#define _MSC_LOG(level, prefix, desc, ...) \
	do \
	{ \
		auto* loggerHandler = ::mediasoupclient::Logger::handler; \
		if (loggerHandler && \
		    ::mediasoupclient::Logger::logLevel >= (level)) \
		{ \
			const int loggerWritten = std::snprintf( \
				::mediasoupclient::Logger::buffer, \
				::mediasoupclient::Logger::BufferSize, \
				prefix _MSC_LOG_STR_DESC desc, \
				_MSC_LOG_ARG, \
				##__VA_ARGS__); \
			if (loggerWritten >= 0) \
			{ \
				const size_t loggerLength = \
					static_cast<size_t>(loggerWritten) < \
					::mediasoupclient::Logger::BufferSize \
						? static_cast<size_t>(loggerWritten) \
						: ::mediasoupclient::Logger::BufferSize - 1; \
				loggerHandler->OnLog( \
					level, \
					::mediasoupclient::Logger::buffer, \
					loggerLength); \
			} \
		} \
	} while (false)

/*
 * TRACE
 */

#ifdef MSC_LOG_TRACE

	#define MSC_TRACE() \
		do \
		{ \
			auto* loggerHandler = ::mediasoupclient::Logger::handler; \
			if (loggerHandler && \
			    ::mediasoupclient::Logger::logLevel >= \
			        ::mediasoupclient::Logger::LogLevel::LOG_TRACE) \
			{ \
				const int loggerWritten = std::snprintf( \
					::mediasoupclient::Logger::buffer, \
					::mediasoupclient::Logger::BufferSize, \
					"[TRACE]" _MSC_LOG_STR, \
					_MSC_LOG_ARG); \
				if (loggerWritten >= 0) \
				{ \
					const size_t loggerLength = \
						static_cast<size_t>(loggerWritten) < \
						::mediasoupclient::Logger::BufferSize \
							? static_cast<size_t>(loggerWritten) \
							: ::mediasoupclient::Logger::BufferSize - 1; \
					loggerHandler->OnLog( \
						::mediasoupclient::Logger::LogLevel::LOG_TRACE, \
						::mediasoupclient::Logger::buffer, \
						loggerLength); \
				} \
			} \
		} while (false)

#else

	#define MSC_TRACE() \
		do {} while (false)

#endif

/*
 * DEBUG
 */

#define MSC_DEBUG(desc, ...) \
	_MSC_LOG( \
		::mediasoupclient::Logger::LogLevel::LOG_DEBUG, \
		"[DEBUG]", \
		desc, \
		##__VA_ARGS__)

/*
 * WARN
 */

#define MSC_WARN(desc, ...) \
	_MSC_LOG( \
		::mediasoupclient::Logger::LogLevel::LOG_WARN, \
		"[WARN]", \
		desc, \
		##__VA_ARGS__)

/*
 * ERROR
 */

#define MSC_ERROR(desc, ...) \
	_MSC_LOG( \
		::mediasoupclient::Logger::LogLevel::LOG_ERROR, \
		"[ERROR]", \
		desc, \
		##__VA_ARGS__)

/*
 * DUMP
 */

#define MSC_DUMP(desc, ...) \
	do \
	{ \
		std::fprintf( \
			stdout, \
			_MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, \
			_MSC_LOG_ARG, \
			##__VA_ARGS__); \
		std::fflush(stdout); \
	} while (false)

/*
 * ABORT
 */

#define MSC_ABORT(desc, ...) \
	do \
	{ \
		std::fprintf( \
			stderr, \
			"[ABORT]" _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, \
			_MSC_LOG_ARG, \
			##__VA_ARGS__); \
		std::fflush(stderr); \
		std::abort(); \
	} while (false)

/*
 * ASSERT
 */

#define MSC_ASSERT(condition, desc, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			MSC_ABORT( \
				"failed assertion `%s': " desc, \
				#condition, \
				##__VA_ARGS__); \
		} \
	} while (false)

// NOLINTEND

// clang-format on

#endif // MSC_LOGGER_HPP
