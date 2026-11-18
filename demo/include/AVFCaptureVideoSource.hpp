#ifndef MSC_DEMO_AVF_CAPTURE_VIDEO_SOURCE_HPP
#define MSC_DEMO_AVF_CAPTURE_VIDEO_SOURCE_HPP

#include "media/base/video_broadcaster.h"
#include "pc/video_track_source.h"

/**
 * VideoTrackSource backed by a real macOS camera via AVFoundation.
 * The ObjC capture session lives in AVFCaptureVideoSource.mm.
 */
class AVFCaptureVideoSource : public webrtc::VideoTrackSource
{
public:
	static webrtc::scoped_refptr<AVFCaptureVideoSource> Create(int width, int height, int fps);

	AVFCaptureVideoSource();

	~AVFCaptureVideoSource() override;

	void AddOrUpdateSink(
	  webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
	  const webrtc::VideoSinkWants& wants) override;

	void RemoveSink(webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

protected:
	webrtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

private:
	webrtc::VideoBroadcaster broadcaster;
	void* impl{ nullptr }; // opaque pointer to ObjC AVFCaptureImpl
};

#endif
