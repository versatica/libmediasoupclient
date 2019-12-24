#ifndef MSC_TEST_VCM_CAPTURER_HPP
#define MSC_TEST_VCM_CAPTURER_HPP

#include "VideoCapturer.hpp"
#include "modules/video_capture/video_capture.h"
#include "api/scoped_refptr.h"
#include <memory>
#include <vector>

class VcmCapturer : public VideoCapturer, public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
	static VcmCapturer* Create(size_t width, size_t height, size_t target_fps, size_t capture_device_index);
	virtual ~VcmCapturer();

	void OnFrame(const webrtc::VideoFrame& frame) override;

private:
	VcmCapturer();
	bool Init(size_t width, size_t height, size_t target_fps, size_t capture_device_index);
	void Destroy();

	rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
	webrtc::VideoCaptureCapability capability_;
};

#endif
