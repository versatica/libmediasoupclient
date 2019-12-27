#ifndef MSC_TEST_VIDEO_CAPTURER_HPP
#define MSC_TEST_VIDEO_CAPTURER_HPP

#include <api/video/video_frame.h>
#include <api/video/video_source_interface.h>
#include <media/base/video_adapter.h>
#include <media/base/video_broadcaster.h>
#include <memory>
#include <stddef.h>

class VideoCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
public:
	VideoCapturer();
	virtual ~VideoCapturer();

	void AddOrUpdateSink(
	  rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
	void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

protected:
	void OnFrame(const webrtc::VideoFrame& frame);
	rtc::VideoSinkWants GetSinkWants();

private:
	void UpdateVideoAdapter();

	rtc::VideoBroadcaster broadcaster;
	cricket::VideoAdapter videoAdapter;
};

#endif
