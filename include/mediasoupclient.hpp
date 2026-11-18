#ifndef MEDIASOUP_CLIENT_HPP
#define MEDIASOUP_CLIENT_HPP

// NOTE: This is the public umbrella header of the library, so the headers below
// are re-exported to whoever includes it.
// IWYU pragma: begin_exports
#include "Device.hpp"
#include "Logger.hpp"
// IWYU pragma: end_exports

namespace mediasoupclient
{
	void Initialize();

	void Cleanup();

	std::string Version();
} // namespace mediasoupclient

#endif
