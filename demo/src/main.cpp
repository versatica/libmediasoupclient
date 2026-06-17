/**
 * mediasoupclient demo
 *
 * Connects to a mediasoup-demo server, sends mic audio and camera
 * video, and receives audio/video from other participants.
 * Runs until Ctrl+C.
 *
 * Usage:
 *   mediasoupclient_demo --ws-url [serverUrl]  --origin [origin]
 */

#include "AVFCaptureVideoSource.hpp"
#include "RoomClient.hpp"
#include "Utils.hpp"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "rtc_base/thread.h"
#include <CLI/CLI.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// Graceful shutdown on Ctrl+C
// ---------------------------------------------------------------------------

static std::atomic<bool> Running{ true };

static void onSignal(int /*unused*/)
{
	Running = false;
}

// ---------------------------------------------------------------------------
// PeerConnectionFactory
// ---------------------------------------------------------------------------

static webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createFactory()
{
	auto* network   = webrtc::Thread::CreateWithSocketServer().release();
	auto* worker    = webrtc::Thread::Create().release();
	auto* signaling = webrtc::Thread::Create().release();

	network->SetName("demo_network", nullptr);
	worker->SetName("demo_worker", nullptr);
	signaling->SetName("demo_signaling", nullptr);

	network->Start();
	worker->Start();
	signaling->Start();

	return webrtc::CreatePeerConnectionFactory(
	  network,
	  worker,
	  signaling,
	  nullptr,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	  std::make_unique<webrtc::VideoEncoderFactoryTemplate<
	    webrtc::LibvpxVp8EncoderTemplateAdapter,
	    webrtc::LibvpxVp9EncoderTemplateAdapter,
	    webrtc::OpenH264EncoderTemplateAdapter,
	    webrtc::LibaomAv1EncoderTemplateAdapter>>(),
	  std::make_unique<webrtc::VideoDecoderFactoryTemplate<
	    webrtc::LibvpxVp8DecoderTemplateAdapter,
	    webrtc::LibvpxVp9DecoderTemplateAdapter,
	    webrtc::OpenH264DecoderTemplateAdapter,
	    webrtc::Dav1dDecoderTemplateAdapter>>(),
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	CLI::App app{ "mediasoupclient demo — send/receive media via a mediasoup-demo server" };

	std::string wsUrl;
	std::string origin;
	std::string roomId = "demo-room";

	app.add_option("--ws-url", wsUrl, "Protoo WebSocket URL (e.g. wss://localhost:4443)")->required();
	app.add_option("--origin", origin, "HTTP Origin header  (e.g. https://myserver:4443)")->required();
	app.add_option("--room-id", roomId, "Room ID")->capture_default_str();

	CLI11_PARSE(app, argc, argv);

	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);

	const std::string peerId = mediasoupclient::Utils::GetRandomString(8);
	const std::string url    = wsUrl + "/?roomId=" + roomId + "&peerId=" + peerId;

	mediasoupclient::Initialize();

	auto factory = createFactory();

	if (!factory)
	{
		std::cerr << "[demo] FAILED: could not create PeerConnectionFactory\n";

		return 1;
	}

	// -- Room --
	RoomClient room(
	  url,
	  origin,
	  "mediasoupclient-demo",
	  factory.get(),
	  {},
	  [](const std::string& peerId, webrtc::VideoTrackInterface*)
	  {
		  std::cout << "[demo] video track from " << peerId << "\n";
	  },
	  [](const std::string& peerId)
	  {
		  std::cout << "[demo] peer left: " << peerId << "\n";
	  },
	  [](const std::string& msg)
	  {
		  std::cout << "[data] " << msg << "\n";
	  });

	try
	{
		room.Join();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[demo] FAILED: join: " << e.what() << "\n";

		return 1;
	}

	// -- Mic --
	try
	{
		room.EnableMic();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[demo] FAILED: enableMic: " << e.what() << "\n";
	}

	// -- Camera --
	auto videoSource = AVFCaptureVideoSource::Create(640, 480, 30);

	if (videoSource)
	{
		auto videoTrack = factory->CreateVideoTrack(videoSource, "camera");

		try
		{
			room.EnableCamera(videoTrack);
		}
		catch (const std::exception& e)
		{
			std::cerr << "[demo] FAILED: enableCamera: " << e.what() << "\n";
		}
	}

	// -- Chat --
	try
	{
		room.EnableChat();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[demo] FAILED: enableChat: " << e.what() << "\n";
	}

	std::cout << "[demo] running — press Ctrl+C to quit\n";

	while (Running)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << "\n[demo] shutting down...\n";

	room.Close();

	std::cout << "[demo] done\n";

	return 0;
}
