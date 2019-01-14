#define MSC_CLASS "Producer"
// #define MSC_LOG_DEV

#include "Producer.hpp"

#include "Exception.hpp"
#include "Logger.hpp"
#include "VideoLayers.hpp"
#include <utility>

namespace mediasoupclient
{
Producer::Producer(
  Producer::Listener* listener,
  Producer::PublicListener* publicListener,
  std::string id,
  webrtc::MediaStreamTrackInterface* track,
  json rtpParameters,
  std::string maxSpatialLayer,
  json appData)
  : listener(listener), publicListener(publicListener), id(std::move(id)), track(track),
    rtpParameters(std::move(rtpParameters)), maxSpatialLayer(std::move(maxSpatialLayer)), appData(std::move(appData))
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

	this->listener->OnClose(this);
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

	this->publicListener->OnTransportClose();
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
std::future<void> Producer::ReplaceTrack(webrtc::MediaStreamTrackInterface* track)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	if (this->closed)
		return reject(Exception("Invalid state"));
	if (track == nullptr)
		return reject(Exception("Missing track"));
	if (track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded)
		return reject(Exception("Track ended"));

	try
	{
		this->listener->OnReplaceTrack(this, track).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	auto paused = this->IsPaused();

	// Set the new track.
	this->track = track;

	// Enable/Disable the new track according to current pause state.
	this->track->set_enabled(!paused);

	promise.set_value();

	return promise.get_future();
}

/**
 * Sets the max spatial layer to be sent.
 */
std::future<void> Producer::SetMaxSpatialLayer(const std::string& spatialLayer)
{
	MSC_TRACE();

	// Returned future's promise.
	std::promise<void> promise;

	auto reject = [&promise](const Exception& error) {
		promise.set_exception(std::make_exception_ptr(error));

		return promise.get_future();
	};

	if (this->closed)
		return reject(Exception("Invalid state"));
	if (this->track->kind() != "video")
		return reject(Exception("Not a video Producer"));
	if (!isValidSpatialLayer(spatialLayer))
		return reject(Exception("Invalid spatial layer"));

	if (spatialLayer == this->maxSpatialLayer)
	{
		promise.set_value();

		return promise.get_future();
	}

	try
	{
		this->listener->OnSetMaxSpatialLayer(this, spatialLayer).get();
	}
	catch (Exception& error)
	{
		return reject(error);
	}

	this->maxSpatialLayer = spatialLayer;

	promise.set_value();

	return promise.get_future();
}
} // namespace mediasoupclient
