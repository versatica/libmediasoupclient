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
#include <fstream>//logfile

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
			virtual void OnLog(LogLevel level, char* payload, size_t len) = 0;
		};

		class DefaultLogHandler : public LogHandlerInterface
		{
			void OnLog(LogLevel level, char* payload, size_t len) override;
		};

		static void SetLogLevel(LogLevel level);
		static void SetHandler(LogHandlerInterface* handler);
		static void SetDefaultHandler();
		static void setLogPath(const char* pFilePath);

	public:
		static LogLevel logLevel;
		static LogHandlerInterface* handler;
		static const size_t bufferSize{ 50000 };
		static char buffer[];
		static std::ofstream stream;
	};
} // namespace mediasoupclient

// clang-format off

/* Logging macros. */

using Logger = mediasoupclient::Logger;

#define _MSC_LOG_SEPARATOR_CHAR "\n"

#ifndef MSC_CLASS
#define MSC_CLASS "placeholder classname"
#endif

#include <sys/timeb.h>
#include <time.h>
template <typename T>
T log_Time(void)
{
	struct tm* ptm;
	struct timeb stTimeb;
	static char szTime[19];

	ftime(&stTimeb);
	ptm = localtime(&stTimeb.time);
	sprintf(
	  szTime,
	  "%02d-%02d %02d:%02d:%02d.%03d",
	  ptm->tm_mon + 1,
	  ptm->tm_mday,
	  ptm->tm_hour,
	  ptm->tm_min,
	  ptm->tm_sec,
	  stTimeb.millitm);
	szTime[18] = 0;
	return szTime;
}

#include <sys/timeb.h>//use for timeb
#define TIME_DESC\
			time_t tt;\
			time(&tt);\
			struct tm * ti = localtime(&tt);\
			std::string  timedesc = std::string(asctime(ti));\
			timedesc[timedesc.length() - 1] = '\0';\
			struct timeb stTimeb;\
			ftime(&stTimeb);\
			char szbuf[1024] = { 0 };\
			snprintf(szbuf, 1024, "%s: %02d", timedesc.c_str(), stTimeb.millitm);\
			timedesc = szbuf;

#ifdef MSC_LOG_FILE_LINE
	#define _MSC_LOG_STR " %s:%d | %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "
	#define _MSC_FILE (std::strchr(__FILE__, '/') ? std::strchr(__FILE__, '/') + 1 : __FILE__)
	#define _MSC_LOG_ARG _MSC_FILE, __LINE__, MSC_CLASS, __FUNCTION__
#else
	#define _MSC_LOG_STR " [%s] %s::%s()"
	#define _MSC_LOG_STR_DESC _MSC_LOG_STR " | "
	#define _MSC_LOG_ARG timedesc.c_str(),MSC_CLASS, __FUNCTION__
#endif

#ifdef MSC_LOG_TRACE
	#define MSC_TRACE() \
		do \
		{ \
			if (Logger::handler && Logger::logLevel >= Logger::LogLevel::LOG_TRACE) \
			{ \
				TIME_DESC\
				int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[TRACE]" _MSC_LOG_STR, _MSC_LOG_ARG); \
				Logger::handler->OnLog(Logger::LogLevel::LOG_TRACE, Logger::buffer, loggerWritten); \
			} \
		} \
		while (false)
#else
	#define MSC_TRACE() ;
#endif

#define MSC_DEBUG(desc, ...) \
	do \
	{ \
		if (Logger::handler && Logger::logLevel >= Logger::LogLevel::LOG_DEBUG) \
		{ \
			TIME_DESC\
			int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[DEBUG]" _MSC_LOG_STR_DESC desc,_MSC_LOG_ARG, ##__VA_ARGS__); \
			Logger::handler->OnLog(Logger::LogLevel::LOG_DEBUG, Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_WARN(desc, ...) \
	do \
	{ \
		if (Logger::handler && Logger::logLevel >= Logger::LogLevel::LOG_WARN) \
		{ \
			TIME_DESC\
			int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[WARN]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			Logger::handler->OnLog(Logger::LogLevel::LOG_WARN, Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_ERROR(desc, ...) \
	do \
	{ \
		if (Logger::handler && Logger::logLevel >= Logger::LogLevel::LOG_ERROR) \
		{ \
			TIME_DESC\
			int loggerWritten = std::snprintf(Logger::buffer, Logger::bufferSize, "[ERROR]" _MSC_LOG_STR_DESC desc, _MSC_LOG_ARG, ##__VA_ARGS__); \
			Logger::handler->OnLog(Logger::LogLevel::LOG_ERROR, Logger::buffer, loggerWritten); \
		} \
	} \
	while (false)

#define MSC_DUMP(desc, ...) \
	do \
	{ \
		TIME_DESC\
		std::fprintf(stdout, _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, _MSC_LOG_ARG, ##__VA_ARGS__); \
		std::fflush(stdout); \
	} \
	while (false)

#define MSC_ABORT(desc, ...) \
	do \
	{ \
		TIME_DESC\
		std::fprintf(stderr, "[ABORT]" _MSC_LOG_STR_DESC desc _MSC_LOG_SEPARATOR_CHAR, _MSC_LOG_ARG, ##__VA_ARGS__); \
		std::fflush(stderr); \
		std::abort(); \
	} \
	while (false)

#define MSC_ASSERT(condition, desc, ...) \
	if (!(condition)) \
	{ \
		TIME_DESC\
		MSC_ABORT("failed assertion `%s': " desc, #condition, ##__VA_ARGS__); \
	}

// clang-format on

#endif
