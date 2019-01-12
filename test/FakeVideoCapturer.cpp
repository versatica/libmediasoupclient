/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fakevideocapturer.h"

#include "rtc_base/arraysize.h"

namespace cricket {
	FakeVideoCapturer::FakeVideoCapturer(bool isScreencast)
	  : running_(false), is_screencast_(isScreencast), rotation_(webrtc::kVideoRotation_0)
	{
		// Default supported formats. Use ResetSupportedFormats to over write.
		using cricket::VideoFormat;
		/*
		static const VideoFormat formats[] = {
		    {1280, 720, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
		    {640, 480, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
		    {320, 240, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
		    {160, 120, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
		    {1280, 720, VideoFormat::FpsToInterval(60), cricket::FOURCC_I420},
		};
		*/

		static const VideoFormat formats[] = {
			{ 1920, 1080, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420 },
			{ 1920, 1080, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420 },
			{ 1280, 720, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420 },
			{ 1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420 },
			{ 960, 540, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420 },
			{ 960, 540, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420 },
			{ 640, 480, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420 },
			{ 640, 480, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420 }
		};

		ResetSupportedFormats({ formats, &formats[arraysize(formats)] });
}

FakeVideoCapturer::FakeVideoCapturer() : FakeVideoCapturer(false) {}

FakeVideoCapturer::~FakeVideoCapturer() {
  SignalDestroyed(this);
}

void FakeVideoCapturer::ResetSupportedFormats(
    const std::vector<cricket::VideoFormat>& formats) {
  SetSupportedFormats(formats);
}

bool FakeVideoCapturer::CaptureFrame() {
	return GetCaptureFormat() != nullptr;
}

bool FakeVideoCapturer::CaptureCustomFrame(int /*width*/, int /*height*/)
{
	return true;
}

bool FakeVideoCapturer::CaptureFrame(const webrtc::VideoFrame& /*frame*/)
{
	return running_;
}

cricket::CaptureState FakeVideoCapturer::Start(
    const cricket::VideoFormat& format) {
  running_ = true;
  SetCaptureFormat(&format);
  SetCaptureState(cricket::CS_RUNNING);
  return cricket::CS_RUNNING;
}

void FakeVideoCapturer::Stop() {
  running_ = false;
	SetCaptureFormat(nullptr);
	SetCaptureState(cricket::CS_STOPPED);
}

bool FakeVideoCapturer::IsRunning() {
  return running_;
}

bool FakeVideoCapturer::IsScreencast() const {
  return is_screencast_;
}

bool FakeVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  fourccs->push_back(cricket::FOURCC_I420);
  fourccs->push_back(cricket::FOURCC_MJPG);
  return true;
}

void FakeVideoCapturer::SetRotation(webrtc::VideoRotation rotation) {
  rotation_ = rotation;
}

webrtc::VideoRotation FakeVideoCapturer::GetRotation() {
  return rotation_;
}

}  // namespace cricket
