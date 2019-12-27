#ifndef MEDIASOUP_CLIENT_HPP
#define MEDIASOUP_CLIENT_HPP

#include "Device.hpp"
#include "Logger.hpp"

namespace mediasoupclient
{
	void Initialize();     // NOLINT(readability-identifier-naming)
	void Cleanup();        // NOLINT(readability-identifier-naming)
	std::string Version(); // NOLINT(readability-identifier-naming)
} // namespace mediasoupclient

#endif
