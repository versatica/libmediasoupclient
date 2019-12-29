#define MSC_CLASS "PeerConnectionFactoryWrapper"

#include "peerConnectionUtils.hpp"
#include "MediaSoupClientErrors.hpp"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_video_track_source.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include <iostream>

using namespace mediasoupclient;

// Create PeerConnectionFactory.
PeerConnectionFactoryWrapper::PeerConnectionFactoryWrapper()
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;

	this->networkThread = rtc::Thread::CreateWithSocketServer();
	this->signalingThread = rtc::Thread::Create();
	this->workerThread = rtc::Thread::Create();

	this->networkThread->SetName("test:network_thread", nullptr);
	this->signalingThread->SetName("test:signaling_thread", nullptr);
	this->workerThread->SetName("test_worker_thread", nullptr);

	if (!this->networkThread->Start() || !this->signalingThread->Start() || !this->workerThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

  auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
  if (!fakeAudioCaptureModule) {
		MSC_THROW_INVALID_STATE_ERROR("audio capture module creation errored");
  }

	this->factory = webrtc::CreatePeerConnectionFactory(
			this->networkThread.get(),
			this->workerThread.get(),
			this->signalingThread.get(),
			fakeAudioCaptureModule,
			webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			webrtc::CreateBuiltinVideoEncoderFactory(),
			webrtc::CreateBuiltinVideoDecoderFactory(),
			nullptr /*audio_mixer*/,
			nullptr /*audio_processing*/);

	if (!this->factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> PeerConnectionFactoryWrapper::CreateAudioTrack(const std::string& label)
{
	if (!this->factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}

	cricket::AudioOptions options;
	options.highpass_filter = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
		this->factory->CreateAudioSource(options);

  return this->factory->CreateAudioTrack(label, source);
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionFactoryWrapper::CreateVideoTrack(const std::string& label)
{
	if (!this->factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}

  return this->factory->CreateVideoTrack(label, webrtc::FakeVideoTrackSource::Create());
}
