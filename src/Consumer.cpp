#define MSC_CLASS "Consumer"
// #define MSC_LOG_DEV

#include "Consumer.hpp"

#include "Exception.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
Consumer::Consumer(
  Consumer::PrivateListener* privateListener,
  Consumer::Listener* listener,
  std::string id,
  std::string localId,
  std::string producerId,
  webrtc::MediaStreamTrackInterface* track,
  json rtpParameters,
  json appData)
  : privateListener(privateListener), listener(listener), id(std::move(id)),
    localId(std::move(localId)), producerId(std::move(producerId)), track(track),
    rtpParameters(std::move(rtpParameters)), appData(std::move(appData))
{
	MSC_TRACE();
}

/**
 * Closes the Consumer.
 */
void Consumer::Close()
{
	MSC_TRACE();

	if (this->closed)
		return;

	this->closed = true;

	this->privateListener->OnClose(this);
}

/**
 * Transport was closed.
 */
void Consumer::TransportClosed()
{
	MSC_TRACE();

	if (this->closed)
		return;

	this->closed = true;

	this->listener->OnTransportClose(this);
}

/**
 * Pauses sending media.
 */
void Consumer::Pause()
{
	MSC_TRACE();

	if (this->closed)
		return;

	this->track->set_enabled(false);
}

/**
 * Resumes sending media.
 */
void Consumer::Resume()
{
	MSC_TRACE();

	if (this->closed)
	{
		MSC_ERROR("consumer closed");

		return;
	}

	this->track->set_enabled(true);
}
} // namespace mediasoupclient
