#ifndef MSC_MEDIASOUP_CLIENT_ERRORS_HPP
#define MSC_MEDIASOUP_CLIENT_ERRORS_HPP

#include "Logger.hpp"
#include <cstdio> // std::snprintf()
#include <stdexcept>

class MediaSoupClientError : public std::runtime_error
{
public:
	explicit MediaSoupClientError(const char* description);
};

/* Inline methods. */

inline MediaSoupClientError::MediaSoupClientError(const char* description)
  : std::runtime_error(description)
{
}

class MediaSoupClientTypeError : public MediaSoupClientError
{
public:
	explicit MediaSoupClientTypeError(const char* description);
};

/* Inline methods. */

inline MediaSoupClientTypeError::MediaSoupClientTypeError(const char* description)
  : MediaSoupClientError(description)
{
}

class MediaSoupClientUnsupportedError : public MediaSoupClientError
{
public:
	explicit MediaSoupClientUnsupportedError(const char* description);
};

/* Inline methods. */

inline MediaSoupClientUnsupportedError::MediaSoupClientUnsupportedError(const char* description)
  : MediaSoupClientError(description)
{
}

class MediaSoupClientInvalidStateError : public MediaSoupClientError
{
public:
	explicit MediaSoupClientInvalidStateError(const char* description);
};

/* Inline methods. */

inline MediaSoupClientInvalidStateError::MediaSoupClientInvalidStateError(const char* description)
  : MediaSoupClientError(description)
{
}

// clang-format off
#define MSC_THROW_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientError(buffer); \
	} while (false)

#define MSC_THROW_TYPE_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientTypeError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientTypeError(buffer); \
	} while (false)

#define MSC_THROW_UNSUPPORTED_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientUnsupportedError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientUnsupportedError(buffer); \
	} while (false)

#define MSC_THROW_INVALID_STATE_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientInvalidStateError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientInvalidStateError(buffer); \
	} while (false)

// clang-format on

#endif
