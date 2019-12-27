#include "VcmCapturer.hpp"
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <cstdint>
#include <memory>
#include <modules/video_capture/video_capture_factory.h>

VcmCapturer::VcmCapturer() : vcm(nullptr)
{
}

bool VcmCapturer::Init(size_t width, size_t height, size_t targetFps, size_t captureDeviceIndex)
{
	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> deviceInfo(
	  webrtc::VideoCaptureFactory::CreateDeviceInfo());

	char deviceName[256];
	char uniqueName[256];
	if (
	  deviceInfo->GetDeviceName(
	    static_cast<uint32_t>(captureDeviceIndex),
	    deviceName,
	    sizeof(deviceName),
	    uniqueName,
	    sizeof(uniqueName)) != 0)
	{
		Destroy();
		return false;
	}

	this->vcm = webrtc::VideoCaptureFactory::Create(uniqueName);
	this->vcm->RegisterCaptureDataCallback(this);

	deviceInfo->GetCapability(this->vcm->CurrentDeviceName(), 0, this->capability);

	this->capability.width     = static_cast<int32_t>(width);
	this->capability.height    = static_cast<int32_t>(height);
	this->capability.maxFPS    = static_cast<int32_t>(targetFps);
	this->capability.videoType = webrtc::VideoType::kI420;

	if (this->vcm->StartCapture(this->capability) != 0)
	{
		Destroy();
		return false;
	}

	RTC_CHECK(this->vcm->CaptureStarted());

	return true;
}

VcmCapturer* VcmCapturer::Create(size_t width, size_t height, size_t targetFps, size_t captureDeviceIndex)
{
	std::unique_ptr<VcmCapturer> vcmCapturer(new VcmCapturer());

	if (!vcmCapturer->Init(width, height, targetFps, captureDeviceIndex))
	{
		RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << width << ", h = " << height
		                    << ", fps = " << targetFps << ")";
		return nullptr;
	}
	return vcmCapturer.release();
}

void VcmCapturer::Destroy()
{
	if (!this->vcm)
		return;

	this->vcm->StopCapture();
	this->vcm->DeRegisterCaptureDataCallback();
	// Release reference to VCM.
	this->vcm = nullptr;
}

VcmCapturer::~VcmCapturer()
{
	Destroy();
}

void VcmCapturer::OnFrame(const webrtc::VideoFrame& frame)
{
	VideoCapturer::OnFrame(frame);
}
