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
  Consumer::Listener* listener,
  Consumer::PublicListener* publicListener,
  std::string id,
  std::string producerId,
  webrtc::MediaStreamTrackInterface* track,
  json rtpParameters,
  json appData)
  : listener(listener), publicListener(publicListener), id(std::move(id)),
    producerId(std::move(producerId)), track(track), rtpParameters(std::move(rtpParameters)),
    appData(std::move(appData))
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

	this->listener->OnClose(this);
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

	this->publicListener->OnTransportClose();
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
