#define MSC_CLASS "Producer"
// #define MSC_LOG_DEV

#include "Producer.hpp"

#include "Exception.hpp"
#include "Logger.hpp"
#include <utility>

using json = nlohmann::json;

namespace mediasoupclient
{
Producer::Producer(
  Producer::PrivateListener* privateListener,
  Producer::Listener* listener,
  const std::string& id,
  const std::string& localId,
  webrtc::MediaStreamTrackInterface* track,
  const json& rtpParameters,
  const json& appData)
  : privateListener(privateListener), listener(listener), id(id), localId(localId), track(track),
    rtpParameters(rtpParameters), appData(appData)
{
	MSC_TRACE();
}

/**
 * Closes the Producer.
 */
void Producer::Close()
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
void Producer::TransportClosed()
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
void Producer::Pause()
{
	MSC_TRACE();

	if (this->closed)
		return;

	this->track->set_enabled(false);
}

/**
 * Resumes sending media.
 */
void Producer::Resume()
{
	MSC_TRACE();

	if (this->closed)
	{
		MSC_ERROR("producer closed");

		return;
	}

	this->track->set_enabled(true);
}

/**
 * Replaces the current track with a new one.
 */
void Producer::ReplaceTrack(webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");
	if (track == nullptr)
		throw Exception("Missing track");
	if (track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded)
		throw Exception("Track ended");

	// May throw.
	this->privateListener->OnReplaceTrack(this, track);

	auto paused = this->IsPaused();

	// Set the new track.
	this->track = track;

	// Enable/Disable the new track according to current pause state.
	this->track->set_enabled(!paused);
}

/**
 * Sets the max spatial layer to be sent.
 */
void Producer::SetMaxSpatialLayer(const uint8_t spatialLayer)
{
	MSC_TRACE();

	if (this->closed)
		throw Exception("Invalid state");
	if (this->track->kind() != "video")
		throw Exception("Not a video Producer");

	if (spatialLayer == this->maxSpatialLayer)
		return;

	// May throw.
	this->privateListener->OnSetMaxSpatialLayer(this, spatialLayer);

	this->maxSpatialLayer = spatialLayer;
}
} // namespace mediasoupclient
