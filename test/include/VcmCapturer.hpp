#ifndef MSC_TEST_VCM_CAPTURER_HPP
#define MSC_TEST_VCM_CAPTURER_HPP

#include "VideoCapturer.hpp"
#include <api/scoped_refptr.h>
#include <memory>
#include <modules/video_capture/video_capture.h>
#include <vector>

class VcmCapturer : public VideoCapturer, public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
	static VcmCapturer* Create(size_t width, size_t height, size_t targetFps, size_t captureDeviceIndex);
	virtual ~VcmCapturer();

	void OnFrame(const webrtc::VideoFrame& frame) override;

private:
	VcmCapturer();
	bool Init(size_t width, size_t height, size_t targetFps, size_t captureDeviceIndex);
	void Destroy();

	rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm{ nullptr };
	webrtc::VideoCaptureCapability capability;
};

#endif
