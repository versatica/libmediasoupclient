/**
 * AVFCaptureVideoSource.mm — macOS camera capture via AVFoundation.
 * Converts CMSampleBuffer frames to WebRTC VideoFrames and feeds them
 * into a VideoBroadcaster so WebRTC sinks receive them.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "AVFCaptureVideoSource.hpp"
#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/time_utils.h"
#include "api/peer_connection_interface.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include <iostream>

// ---------------------------------------------------------------------------
// ObjC frame delegate
// ---------------------------------------------------------------------------

@interface AVFCaptureDelegate
    : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
- (instancetype)initWithBroadcaster:(webrtc::VideoBroadcaster*)broadcaster;
@end

@implementation AVFCaptureDelegate {
	webrtc::VideoBroadcaster* _broadcaster;
}

- (instancetype)initWithBroadcaster:(webrtc::VideoBroadcaster*)broadcaster
{
	self          = [super init];
	_broadcaster  = broadcaster;
	return self;
}

- (void)captureOutput:(AVCaptureOutput*)output
  didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection*)connection
{
	CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
	if (!pixelBuffer)
		return;

	CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

	const int width  = (int)CVPixelBufferGetWidth(pixelBuffer);
	const int height = (int)CVPixelBufferGetHeight(pixelBuffer);
	OSType fmt       = CVPixelBufferGetPixelFormatType(pixelBuffer);

	auto i420 = webrtc::I420Buffer::Create(width, height);

	if (fmt == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
	    fmt == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
	{
		// NV12
		const uint8_t* y  = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
		const uint8_t* uv = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
		libyuv::NV12ToI420(
		  y, (int)CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0),
		  uv, (int)CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1),
		  i420->MutableDataY(), i420->StrideY(),
		  i420->MutableDataU(), i420->StrideU(),
		  i420->MutableDataV(), i420->StrideV(),
		  width, height);
	}
	else if (fmt == kCVPixelFormatType_32BGRA)
	{
		const uint8_t* bgra = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
		libyuv::ARGBToI420(
		  bgra, (int)CVPixelBufferGetBytesPerRow(pixelBuffer),
		  i420->MutableDataY(), i420->StrideY(),
		  i420->MutableDataU(), i420->StrideU(),
		  i420->MutableDataV(), i420->StrideV(),
		  width, height);
	}
	else
	{
		CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
		return; // unsupported format
	}

	CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

	webrtc::VideoFrame frame =
	  webrtc::VideoFrame::Builder()
	    .set_video_frame_buffer(i420)
	    .set_timestamp_us(webrtc::TimeMicros())
	    .build();

	_broadcaster->OnFrame(frame);
}
@end

// ---------------------------------------------------------------------------
// ObjC capture session wrapper
// ---------------------------------------------------------------------------

@interface AVFCaptureImpl : NSObject
- (instancetype)initWithBroadcaster:(webrtc::VideoBroadcaster*)broadcaster
                              width:(int)width
                             height:(int)height
                                fps:(int)fps;
- (BOOL)start;
- (void)stop;
@end

@implementation AVFCaptureImpl {
	AVCaptureSession* _session;
	AVFCaptureDelegate* _delegate;
	dispatch_queue_t _queue;
}

- (instancetype)initWithBroadcaster:(webrtc::VideoBroadcaster*)broadcaster
                              width:(int)width
                             height:(int)height
                                fps:(int)fps
{
	self = [super init];
	if (!self)
		return nil;

	_queue    = dispatch_queue_create("avf_capture", DISPATCH_QUEUE_SERIAL);
	_delegate = [[AVFCaptureDelegate alloc] initWithBroadcaster:broadcaster];
	_session  = [[AVCaptureSession alloc] init];

	// Camera device
	AVCaptureDevice* device =
	  [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
	if (!device)
	{
		std::cerr << "[video] no camera device found\n";
		return nil;
	}
	std::cout << "[video] camera: " << [device.localizedName UTF8String] << "\n";

	NSError* error = nil;
	AVCaptureDeviceInput* input =
	  [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
	if (error || !input)
	{
		std::cerr << "[video] failed to create camera input\n";
		return nil;
	}

	// Output
	AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
	output.videoSettings = @{
		(NSString*)kCVPixelBufferPixelFormatTypeKey :
		  @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
	};
	output.alwaysDiscardsLateVideoFrames = YES;
	[output setSampleBufferDelegate:_delegate queue:_queue];

	[_session beginConfiguration];
	if ([_session canAddInput:input])
		[_session addInput:input];
	if ([_session canAddOutput:output])
		[_session addOutput:output];

	// Frame rate
	AVCaptureConnection* conn =
	  [output connectionWithMediaType:AVMediaTypeVideo];
	if (conn && conn.isVideoMinFrameDurationSupported)
		conn.videoMinFrameDuration = CMTimeMake(1, fps);

	[_session commitConfiguration];
	return self;
}

- (BOOL)start
{
	[_session startRunning];
	return _session.isRunning;
}

- (void)stop
{
	[_session stopRunning];
}
@end

// ---------------------------------------------------------------------------
// C++ AVFCaptureVideoSource
// ---------------------------------------------------------------------------

AVFCaptureVideoSource::AVFCaptureVideoSource()
  : webrtc::VideoTrackSource(/*remote=*/false), impl(nullptr)
{
}

AVFCaptureVideoSource::~AVFCaptureVideoSource()
{
	if (this->impl)
	{
		[(__bridge AVFCaptureImpl*)this->impl stop];
		CFRelease(this->impl);
	}
}

// static
webrtc::scoped_refptr<AVFCaptureVideoSource> AVFCaptureVideoSource::Create(
  int width, int height, int fps)
{
	// Request camera permission
	__block BOOL granted = NO;
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
	                         completionHandler:^(BOOL g) {
		                         granted = g;
		                         dispatch_semaphore_signal(sem);
	                         }];
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

	if (!granted)
	{
		std::cerr << "[video] camera permission denied\n";
		return nullptr;
	}

	auto source = webrtc::make_ref_counted<AVFCaptureVideoSource>();

	AVFCaptureImpl* impl =
	  [[AVFCaptureImpl alloc] initWithBroadcaster:&source->broadcaster
	                                        width:width
	                                       height:height
	                                          fps:fps];
	if (!impl)
		return nullptr;

	source->impl = (void*)CFBridgingRetain(impl);

	if (![impl start])
	{
		std::cerr << "[video] failed to start camera\n";
		return nullptr;
	}

	std::cout << "[video] camera started " << width << "x" << height << "@" << fps << "fps\n";
	return source;
}

webrtc::VideoSourceInterface<webrtc::VideoFrame>* AVFCaptureVideoSource::source()
{
	return &broadcaster;
}

void AVFCaptureVideoSource::AddOrUpdateSink(
  webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
  const webrtc::VideoSinkWants& wants)
{
	broadcaster.AddOrUpdateSink(sink, wants);
}

void AVFCaptureVideoSource::RemoveSink(
  webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
	broadcaster.RemoveSink(sink);
}
