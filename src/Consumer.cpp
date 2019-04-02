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
  const std::string& id,
  const std::string& localId,
  const std::string& producerId,
  webrtc::MediaStreamTrackInterface* track,
  const json& rtpParameters,
  const json& appData)
  : privateListener(privateListener), listener(listener), id(id), localId(localId),
    producerId(producerId), track(track), rtpParameters(rtpParameters), appData(appData)
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
