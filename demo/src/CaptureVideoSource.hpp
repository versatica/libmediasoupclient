#pragma once

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "media/base/video_broadcaster.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include <iostream>
#include <memory>

/**
 * VideoTrackSource backed by a real camera via WebRTC's VideoCaptureModule.
 * Implements VideoSinkInterface so it can be registered directly as the
 * capture callback, and forwards frames to a VideoBroadcaster that
 * distributes them to WebRTC sinks.
 */
class CaptureVideoSource : public webrtc::VideoTrackSource,
                           public webrtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
	static webrtc::scoped_refptr<CaptureVideoSource> Create(int width, int height, int fps)
	{
		auto deviceInfo = std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo>(
		  webrtc::VideoCaptureFactory::CreateDeviceInfo());

		if (!deviceInfo || deviceInfo->NumberOfDevices() == 0)
		{
			std::cerr << "[video] no camera devices found\n";
			return nullptr;
		}

		char name[256];
		char uniqueId[256];
		deviceInfo->GetDeviceName(0, name, sizeof(name), uniqueId, sizeof(uniqueId));
		std::cout << "[video] using camera: " << name << "\n";

		auto capturer = webrtc::VideoCaptureFactory::Create(uniqueId);
		if (!capturer)
		{
			std::cerr << "[video] failed to create capturer\n";
			return nullptr;
		}

		webrtc::VideoCaptureCapability capability;
		capability.width     = width;
		capability.height    = height;
		capability.maxFPS    = fps;
		capability.videoType = webrtc::VideoType::kI420;

		auto source = webrtc::make_ref_counted<CaptureVideoSource>(capturer);

		if (capturer->StartCapture(capability) != 0)
		{
			std::cerr << "[video] failed to start capture\n";
			return nullptr;
		}

		std::cout << "[video] camera started " << width << "x" << height << "@" << fps << "fps\n";
		return source;
	}

	explicit CaptureVideoSource(webrtc::scoped_refptr<webrtc::VideoCaptureModule> capturer)
	  : webrtc::VideoTrackSource(/*remote=*/false), capturer_(capturer)
	{
		capturer_->RegisterCaptureDataCallback(this);
	}

	~CaptureVideoSource() override
	{
		capturer_->StopCapture();
		capturer_->DeRegisterCaptureDataCallback();
	}

	// VideoSinkInterface — called by the capture module with each new frame
	void OnFrame(const webrtc::VideoFrame& frame) override
	{
		broadcaster_.OnFrame(frame);
	}

	// VideoTrackSource
	webrtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
	{
		return &broadcaster_;
	}

protected:
	void AddOrUpdateSink(
	  webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const webrtc::VideoSinkWants& wants) override
	{
		broadcaster_.AddOrUpdateSink(sink, wants);
	}

	void RemoveSink(webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override
	{
		broadcaster_.RemoveSink(sink);
	}

private:
	webrtc::scoped_refptr<webrtc::VideoCaptureModule> capturer_;
	webrtc::VideoBroadcaster broadcaster_;
};
