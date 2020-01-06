#define MSC_CLASS "Producer"

#include "Producer.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"

using json = nlohmann::json;

namespace mediasoupclient
{
	Producer::Producer(
	  Producer::PrivateListener* privateListener,
	  Producer::Listener* listener,
	  const std::string& id,
	  const std::string& localId,
	  webrtc::RtpSenderInterface* rtpSender,
	  webrtc::MediaStreamTrackInterface* track,
	  const json& rtpParameters,
	  const json& appData)
	  : privateListener(privateListener), listener(listener), id(id), localId(localId),
	    rtpSender(rtpSender), track(track), rtpParameters(rtpParameters), appData(appData)
	{
		MSC_TRACE();
	}

	const std::string& Producer::GetId() const
	{
		MSC_TRACE();

		return this->id;
	}

	const std::string& Producer::GetLocalId() const
	{
		MSC_TRACE();

		return this->localId;
	}

	bool Producer::IsClosed() const
	{
		MSC_TRACE();

		return this->closed;
	}

	std::string Producer::GetKind() const
	{
		MSC_TRACE();

		return this->track->kind();
	}

	webrtc::RtpSenderInterface* Producer::GetRtpSender() const
	{
		MSC_TRACE();

		return this->rtpSender;
	}

	webrtc::MediaStreamTrackInterface* Producer::GetTrack() const
	{
		MSC_TRACE();

		return this->track;
	}

	const json& Producer::GetRtpParameters() const
	{
		MSC_TRACE();

		return this->rtpParameters;
	}

	bool Producer::IsPaused() const
	{
		MSC_TRACE();

		return !this->track->enabled();
	}

	uint8_t Producer::GetMaxSpatialLayer() const
	{
		MSC_TRACE();

		return this->maxSpatialLayer;
	}

	json& Producer::GetAppData()
	{
		MSC_TRACE();

		return this->appData;
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

	json Producer::GetStats() const
	{
		if (this->closed)
			MSC_THROW_INVALID_STATE_ERROR("Producer closed");

		return this->privateListener->OnGetStats(this);
	}

	/**
	 * Pauses sending media.
	 */
	void Producer::Pause()
	{
		MSC_TRACE();

		if (this->closed)
		{
			MSC_ERROR("Producer closed");

			return;
		}

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
			MSC_ERROR("Producer closed");

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
			MSC_THROW_INVALID_STATE_ERROR("Producer closed");
		else if (track == nullptr)
			MSC_THROW_TYPE_ERROR("missing track");
		else if (track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded)
			MSC_THROW_INVALID_STATE_ERROR("track ended");

		// Do nothing if this is the same track as the current handled one.
		if (track == this->track)
		{
			MSC_DEBUG("same track, ignored");

			return;
		}

		// May throw.
		this->privateListener->OnReplaceTrack(this, track);

		// Keep current paused state.
		auto paused = IsPaused();

		// Set the new track.
		this->track = track;

		// If this Producer was paused/resumed and the state of the new
		// track does not match, fix it.
		if (!paused)
			this->track->set_enabled(true);
		else
			this->track->set_enabled(false);
	}

	/**
	 * Sets the max spatial layer to be sent.
	 */
	void Producer::SetMaxSpatialLayer(const uint8_t spatialLayer)
	{
		MSC_TRACE();

		if (this->closed)
			MSC_THROW_INVALID_STATE_ERROR("Producer closed");
		else if (this->track->kind() != "video")
			MSC_THROW_TYPE_ERROR("not a video Producer");

		if (spatialLayer == this->maxSpatialLayer)
			return;

		// May throw.
		this->privateListener->OnSetMaxSpatialLayer(this, spatialLayer);

		this->maxSpatialLayer = spatialLayer;
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
} // namespace mediasoupclient
