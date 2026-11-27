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
#include <chrono>
#include <csignal>
#include <iostream>
#include <optional>
#include <string>
#include <sys/select.h> // select(), fd_set, FD_ZERO(), FD_SET()
#include <thread>
#include <unistd.h> // STDIN_FILENO

// ---------------------------------------------------------------------------
// Graceful shutdown on Ctrl+C
// ---------------------------------------------------------------------------

static std::atomic<bool> Running{ true };

static void onSignal(int /*unused*/)
{
	Running = false;
}

// ---------------------------------------------------------------------------
// Interactive commands
// ---------------------------------------------------------------------------

static void printCommands()
{
	std::cout << "[demo] commands: m/M pause/resume mic, c/C pause/resume camera,"
	          << " a/A pause/resume audio consumers, v/V pause/resume video consumers,"
	          << " q quit\n";
}

/**
 * Reads a command from stdin, waiting at most the given time so that Ctrl+C is
 * still handled promptly.
 */
static std::optional<char> readCommand(std::chrono::milliseconds timeout)
{
	fd_set readFds;

	FD_ZERO(&readFds);
	FD_SET(STDIN_FILENO, &readFds);

	struct timeval tv{ .tv_sec = 0, .tv_usec = static_cast<suseconds_t>(timeout.count() * 1000) };

	if (select(STDIN_FILENO + 1, &readFds, nullptr, nullptr, std::addressof(tv)) <= 0)
	{
		return std::nullopt;
	}

	std::string line;

	if (!std::getline(std::cin, line) || line.empty())
	{
		return std::nullopt;
	}

	return line.front();
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

// NOTE: Split from main() so that the latter can guarantee that no exception
// escapes it, which would call std::terminate().
static int run(int argc, char* argv[])
{
	CLI::App app{ "mediasoupclient demo — send/receive media via a mediasoup-demo server" };

	std::string wsUrl;
	std::string origin;
	std::string roomId  = "demo-room";
	bool zeroRtpOnPause = false;

	app.add_option("--ws-url", wsUrl, "Protoo WebSocket URL (e.g. wss://localhost:4443)")->required();
	app.add_option("--origin", origin, "HTTP Origin header  (e.g. https://myserver:4443)")->required();
	app.add_option("--room-id", roomId, "Room ID")->capture_default_str();
	app.add_flag(
	  "--zero-rtp-on-pause",
	  zeroRtpOnPause,
	  "Set the RtpTransceiver of the producers inactive while paused");

	CLI11_PARSE(app, argc, argv);

	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);

	const std::string peerId = mediasoupclient::Utils::GetRandomString(8);
	const std::string url    = wsUrl + "/?roomId=" + roomId + "&peerId=" + peerId;

	mediasoupclient::Initialize();

	const auto factory = createFactory();

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
	  [](const std::string& peerId, webrtc::VideoTrackInterface*) -> void
	  {
		  std::cout << "[demo] video track from " << peerId << "\n";
	  },
	  [](const std::string& peerId) -> void
	  {
		  std::cout << "[demo] peer left: " << peerId << "\n";
	  },
	  [](const std::string& msg) -> void
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
		room.EnableMic(zeroRtpOnPause);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[demo] FAILED: enableMic: " << e.what() << "\n";
	}

	// -- Camera --
	const auto videoSource = AVFCaptureVideoSource::Create(640, 480, 30);

	if (videoSource)
	{
		const auto videoTrack = factory->CreateVideoTrack(videoSource, "camera");

		try
		{
			room.EnableCamera(videoTrack, zeroRtpOnPause);
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

	printCommands();

	while (Running)
	{
		const auto command = readCommand(std::chrono::milliseconds(100));

		if (!command.has_value())
		{
			continue;
		}

		try
		{
			switch (command.value())
			{
				case 'm':
				{
					room.PauseMic();
					break;
				}

				case 'M':
				{
					room.ResumeMic();
					break;
				}

				case 'c':
				{
					room.PauseCamera();
					break;
				}

				case 'C':
				{
					room.ResumeCamera();
					break;
				}

				case 'a':
				{
					room.PauseAudioConsumers();
					break;
				}

				case 'A':
				{
					room.ResumeAudioConsumers();
					break;
				}

				case 'v':
				{
					room.PauseVideoConsumers();
					break;
				}

				case 'V':
				{
					room.ResumeVideoConsumers();
					break;
				}

				case 'q':
				{
					Running = false;
					break;
				}

				default:
				{
					printCommands();
					break;
				}
			}
		}
		catch (const std::exception& error)
		{
			std::cerr << "[demo] FAILED: " << error.what() << "\n";
		}
	}

	std::cout << "\n[demo] shutting down...\n";

	room.Close();

	std::cout << "[demo] done\n";

	return 0;
}

int main(int argc, char* argv[])
{
	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[demo] FAILED: " << e.what() << "\n";

		return 1;
	}
	catch (...)
	{
		std::cerr << "[demo] FAILED: unknown error\n";

		return 1;
	}
}
