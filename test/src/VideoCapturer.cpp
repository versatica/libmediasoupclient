#include "VideoCapturer.hpp"
#include <api/scoped_refptr.h>
#include <api/video/i420_buffer.h>
#include <api/video/video_frame_buffer.h>
#include <api/video/video_rotation.h>
#include <algorithm>

VideoCapturer::VideoCapturer()  = default;
VideoCapturer::~VideoCapturer() = default;

void VideoCapturer::OnFrame(const webrtc::VideoFrame& frame)
{
	int croppedWidth  = 0;
	int croppedHeight = 0;
	int outWidth      = 0;
	int outHeight     = 0;

	if (!this->videoAdapter.AdaptFrameResolution(
	      frame.width(),
	      frame.height(),
	      frame.timestamp_us() * 1000,
	      &croppedWidth,
	      &croppedHeight,
	      &outWidth,
	      &outHeight))
	{
		// Drop frame in order to respect frame rate constraint.
		return;
	}

	// Video adapter has requested a down-scale. Allocate a new buffer and
	// return scaled version.
	if (outHeight != frame.height() || outWidth != frame.width())
	{
		rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer =
		  webrtc::I420Buffer::Create(outWidth, outHeight);

		scaledBuffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());

		this->broadcaster.OnFrame(webrtc::VideoFrame::Builder()
		                            .set_video_frame_buffer(scaledBuffer)
		                            .set_rotation(webrtc::kVideoRotation_0)
		                            .set_timestamp_us(frame.timestamp_us())
		                            .set_id(frame.id())
		                            .build());
	}
	// No adaptations needed, just return the frame as is.
	else
	{
		this->broadcaster.OnFrame(frame);
	}
}

rtc::VideoSinkWants VideoCapturer::GetSinkWants()
{
	return this->broadcaster.wants();
}

void VideoCapturer::AddOrUpdateSink(
  rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
	this->broadcaster.AddOrUpdateSink(sink, wants);
	UpdateVideoAdapter();
}

void VideoCapturer::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
	this->broadcaster.RemoveSink(sink);
	UpdateVideoAdapter();
}

void VideoCapturer::UpdateVideoAdapter()
{
	rtc::VideoSinkWants wants = this->broadcaster.wants();

	this->videoAdapter.OnResolutionFramerateRequest(
	  wants.target_pixel_count, wants.max_pixel_count, wants.max_framerate_fps);
}
