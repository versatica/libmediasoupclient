#ifndef MEDIA_STREAM_TRACK_FACTORY_H
#define MEDIA_STREAM_TRACK_FACTORY_H

#define MSC_CLASS "MediaStreamTrackFactory"

#include "MediaStreamTrackFactory.hpp"
#include "MediaSoupClientErrors.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "mediasoupclient.hpp"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_video_track_source.h"
#include <iostream>

using namespace mediasoupclient;

void MediaStreamTrackFactory::Create()
{
	if (Factory)
		return;
	NetworkThread   = webrtc::Thread::CreateWithSocketServer();
	WorkerThread    = webrtc::Thread::Create();
	SignalingThread = webrtc::Thread::Create();

	NetworkThread->SetName("network_thread", nullptr);
	SignalingThread->SetName("signaling_thread", nullptr);
	WorkerThread->SetName("worker_thread", nullptr);

	if (!NetworkThread->Start() || !SignalingThread->Start() || !WorkerThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
	if (!fakeAudioCaptureModule)
	{
		MSC_THROW_INVALID_STATE_ERROR("audio capture module creation errored");
	}

	Factory = webrtc::CreatePeerConnectionFactory(
	  NetworkThread.get(),
	  WorkerThread.get(),
	  SignalingThread.get(),
	  fakeAudioCaptureModule,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  webrtc::CreateBuiltinVideoEncoderFactory(),
	  webrtc::CreateBuiltinVideoDecoderFactory(),
	  nullptr /*audio_mixer*/,
	  nullptr /*audio_processing*/);

	if (!Factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}
	PeerConnectionOptions.factory = Factory.get();
}

void MediaStreamTrackFactory::ReleaseThreads()
{
	if (Factory)
	{
		PeerConnectionOptions.factory = nullptr;
		Factory                       = nullptr;
	}

	if (NetworkThread)
	{
		NetworkThread->Stop();
		NetworkThread.reset();
		NetworkThread = nullptr;
	}

	if (WorkerThread)
	{
		WorkerThread->Stop();
		WorkerThread.reset();
		WorkerThread = nullptr;
	}

	if (SignalingThread)
	{
		SignalingThread->Stop();
		SignalingThread.reset();
		SignalingThread = nullptr;
	}
}

// Audio track creation.
webrtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	MediaStreamTrackFactory& singleton = MediaStreamTrackFactory::getInstance();

	webrtc::AudioOptions options;
	options.highpass_filter = false;

	webrtc::scoped_refptr<webrtc::AudioSourceInterface> source =
	  singleton.Factory->CreateAudioSource(options);

	return singleton.Factory->CreateAudioTrack(label, source.get());
}

// Video track creation.
webrtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& label)
{
	MediaStreamTrackFactory& singleton = MediaStreamTrackFactory::getInstance();

	webrtc::scoped_refptr<webrtc::FakeVideoTrackSource> source = webrtc::FakeVideoTrackSource::Create();

	return singleton.Factory->CreateVideoTrack(source, label);
}

#endif // MEDIA_STREAM_TRACK_FACTORY_H
