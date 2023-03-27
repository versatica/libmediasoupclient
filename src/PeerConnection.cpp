#define MSC_CLASS "PeerConnection"

#include "PeerConnection.hpp"
#include "Logger.hpp"
#include "MediaSoupClientErrors.hpp"
#include "Utils.hpp"
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <rtc_base/ssl_adapter.h>

using json = nlohmann::json;

namespace mediasoupclient
{
	/* Static. */

	// clang-format off
	std::map<PeerConnection::SdpType, const std::string> PeerConnection::sdpType2String =
	{
		{ PeerConnection::SdpType::OFFER,    "offer"    },
		{ PeerConnection::SdpType::PRANSWER, "pranswer" },
		{ PeerConnection::SdpType::ANSWER,   "answer"   }
	};

	std::map<webrtc::PeerConnectionInterface::IceConnectionState, const std::string>
		PeerConnection::iceConnectionState2String =
	{
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew,          "new"          },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionChecking,     "checking"     },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected,    "connected"    },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted,    "completed"    },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed,       "failed"       },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected, "disconnected" },
		{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionClosed,       "closed"       }
	};

	std::map<webrtc::PeerConnectionInterface::IceGatheringState, const std::string>
		PeerConnection::iceGatheringState2String =
	{
		{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringNew,       "new"       },
		{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering, "gathering" },
		{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete,  "complete"  }
	};

	std::map<webrtc::PeerConnectionInterface::SignalingState, const std::string>
		PeerConnection::signalingState2String =
	{
		{ webrtc::PeerConnectionInterface::SignalingState::kStable,             "stable"               },
		{ webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer,     "have-local-offer"     },
		{ webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer,  "have-local-pranswer"  },
		{ webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer,    "have-remote-offer"    },
		{ webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer, "have-remote-pranswer" },
		{ webrtc::PeerConnectionInterface::SignalingState::kClosed,             "closed"               }
	};
	// clang-format on

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnection::DefaultFactory() {
		static std::once_flag f;
		static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> sharedFactory;

		std::call_once(f, [] {
			rtc::Thread* networkThread   = rtc::Thread::CreateWithSocketServer().release();
			rtc::Thread* signalingThread = rtc::Thread::Create().release();
			rtc::Thread* workerThread    = rtc::Thread::Create().release();

			networkThread->SetName("network_thread", nullptr);
			signalingThread->SetName("signaling_thread", nullptr);
			workerThread->SetName("worker_thread", nullptr);

			if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
			{
				MSC_THROW_INVALID_STATE_ERROR("thread start errored");
			}

			sharedFactory = webrtc::CreatePeerConnectionFactory(
				networkThread,
				workerThread,
				signalingThread,
				nullptr /*default_adm*/,
				webrtc::CreateBuiltinAudioEncoderFactory(),
				webrtc::CreateBuiltinAudioDecoderFactory(),
				webrtc::CreateBuiltinVideoEncoderFactory(),
				webrtc::CreateBuiltinVideoDecoderFactory(),
				nullptr /*audio_mixer*/,
				nullptr /*audio_processing*/
			);
		});

		return sharedFactory;
	}

	/* Instance methods. */

	PeerConnection::PeerConnection(
	  PeerConnection::PrivateListener* privateListener, const PeerConnection::Options& options)
	{
		MSC_TRACE();

		auto config = options.config;
		this->factory = options.factory ? options.factory : PeerConnection::DefaultFactory();

		// Set SDP semantics to Unified Plan.
		config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

		// Create the webrtc::Peerconnection.
		webrtc::PeerConnectionDependencies dependencies(privateListener);
		auto result = this->factory->CreatePeerConnectionOrError(config, std::move(dependencies));

		this->pc = result.MoveValue();
	}

	void PeerConnection::Close()
	{
		MSC_TRACE();

		this->pc->Close();
	}

	webrtc::PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() const
	{
		MSC_TRACE();

		return this->pc->GetConfiguration();
	}

	bool PeerConnection::SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config)
	{
		MSC_TRACE();

		webrtc::RTCError error = this->pc->SetConfiguration(config);

		if (error.ok())
		{
			return true;
		}

		MSC_WARN(
		  "webrtc::PeerConnection::SetConfiguration failed [%s:%s]",
		  webrtc::ToString(error.type()),
		  error.message());

		return false;
	}

	class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver
	{
	public:
		using Callback = std::function<void(std::string, webrtc::RTCError)>;
		CreateSessionDescriptionObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc, Callback fn): pc(pc), callback(fn) {};

		~CreateSessionDescriptionObserver() {
			// Make sure pc isn't destroyed within our callback
			pc->signaling_thread()->PostTask([pc = this->pc] {});
		}

	private:
		/* Virtual methods inherited from webrtc::CreateSessionDescriptionObserver. */
		void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
			if (desc) {
				std::string sdp;
				desc->ToString(&sdp);
				delete desc;

				callback(sdp, webrtc::RTCError::OK());
			} else {
				// Our SDP may be null if the connection was closed during creation
				callback({}, {webrtc::RTCErrorType::INVALID_STATE, "sdp is null"});
			}
		}

		void OnFailure(webrtc::RTCError error) override {
			callback({}, error);
		}
	private:
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc; // keep alive
		Callback callback;
	};

	void PeerConnection::CreateOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& opts, SDPHandler callback) {
		MSC_TRACE();

		this->pc->CreateOffer(rtc::make_ref_counted<CreateSessionDescriptionObserver>(this->pc, callback).get(), opts);
	}

	void PeerConnection::CreateAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& opts, SDPHandler callback) {
		MSC_TRACE();

		this->pc->CreateAnswer(rtc::make_ref_counted<CreateSessionDescriptionObserver>(this->pc, callback).get(), opts);
	}

	namespace {
		webrtc::SdpType WebRtcSdpType(PeerConnection::SdpType t) {
			switch (t) {
				case PeerConnection::SdpType::OFFER:	return webrtc::SdpType::kOffer;
				case PeerConnection::SdpType::PRANSWER:	return webrtc::SdpType::kPrAnswer;
				case PeerConnection::SdpType::ANSWER:	return webrtc::SdpType::kAnswer;
				default:								abort();
			}
		}
	}

	class SetLocalDescriptionObserver : public webrtc::SetLocalDescriptionObserverInterface {
	public:
		using Callback = std::function<void(webrtc::RTCError error)>;

		SetLocalDescriptionObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc, Callback fn): pc(pc), callback(fn) {};

		~SetLocalDescriptionObserver() {
			// Make sure pc isn't destroyed within our callback
			pc->signaling_thread()->PostTask([pc = this->pc] {});
		}

	private:
		void OnSetLocalDescriptionComplete(webrtc::RTCError error) override { callback(error); }
	private:
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc; // keep alive
		Callback callback;
	};

	void PeerConnection::SetLocalDescription(PeerConnection::SdpType type, const std::string& sdp, SDPHandler callback)
	{
		MSC_TRACE();

		webrtc::SdpParseError error;
		auto sessionDescription = webrtc::CreateSessionDescription(WebRtcSdpType(type), sdp, &error);

		if (sessionDescription == nullptr)
		{
			MSC_WARN(
			  "webrtc::CreateSessionDescription failed [%s]: %s",
			  error.line.c_str(),
			  error.description.c_str());

			return this->pc->signaling_thread()->PostTask([callback] {
				callback({}, webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER));
			});
		}

		std::string normalizedSdp;
		sessionDescription->ToString(&normalizedSdp);

		SetLocalDescriptionObserver::Callback wrappedCallback = [normalizedSdp, callback](auto error) {
			callback(error.ok() ? normalizedSdp : "", error);
		};

		this->pc->SetLocalDescription(std::move(sessionDescription), rtc::make_ref_counted<SetLocalDescriptionObserver>(this->pc, wrappedCallback));
	}

	class SetRemoteDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface {
	public:
		using Callback = std::function<void(webrtc::RTCError error)>;

		SetRemoteDescriptionObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc, Callback fn): pc(pc), callback(fn) {};

		~SetRemoteDescriptionObserver() {
			// Make sure pc isn't destroyed within our callback
			pc->signaling_thread()->PostTask([pc = this->pc] {});
		}

	private:
		void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override { callback(error); }
	private:
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc; // keep alive
		Callback callback;
	};

	void PeerConnection::SetRemoteDescription(PeerConnection::SdpType type, const std::string& sdp, SDPHandler callback)
	{
		MSC_TRACE();

		webrtc::SdpParseError error;
		auto sessionDescription = webrtc::CreateSessionDescription(WebRtcSdpType(type), sdp, &error);

		if (sessionDescription == nullptr)
		{
			MSC_WARN(
			  "webrtc::CreateSessionDescription failed [%s]: %s",
			  error.line.c_str(),
			  error.description.c_str());

			return this->pc->signaling_thread()->PostTask([callback] {
				callback({}, webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER));
			});
		}

		std::string normalizedSdp;
		sessionDescription->ToString(&normalizedSdp);

		SetLocalDescriptionObserver::Callback wrappedCallback = [normalizedSdp, callback](auto error) {
			callback(error.ok() ? normalizedSdp : "", error);
		};

		this->pc->SetRemoteDescription(std::move(sessionDescription), rtc::make_ref_counted<SetRemoteDescriptionObserver>(this->pc, wrappedCallback));
	}

	const std::string PeerConnection::GetLocalDescription()
	{
		MSC_TRACE();
		
		// TODO
		// assert(this->pc->signaling_thread()->IsCurrent());

		auto desc = this->pc->local_description();
		std::string sdp;

		desc->ToString(&sdp);

		return sdp;
	}

	const std::string PeerConnection::GetRemoteDescription()
	{
		MSC_TRACE();

		// TODO
		// assert(this->pc->signaling_thread()->IsCurrent());

		auto desc = this->pc->remote_description();
		std::string sdp;

		desc->ToString(&sdp);

		return sdp;
	}

	std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> PeerConnection::GetTransceivers() const
	{
		MSC_TRACE();

		return this->pc->GetTransceivers();
	}

	rtc::scoped_refptr<webrtc::RtpTransceiverInterface> PeerConnection::AddTransceiver(
	  cricket::MediaType mediaType)
	{
		MSC_TRACE();

		auto result = this->pc->AddTransceiver(mediaType);

		if (!result.ok())
		{
			rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = nullptr;

			return transceiver;
		}

		return result.value();
	}

	rtc::scoped_refptr<webrtc::RtpTransceiverInterface> PeerConnection::AddTransceiver(
	  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
	  webrtc::RtpTransceiverInit rtpTransceiverInit)
	{
		MSC_TRACE();

		/*
		 * Define a stream id so the generated local description is correct.
		 * - with a stream id:    "a=ssrc:<ssrc-id> mslabel:<value>"
		 * - without a stream id: "a=ssrc:<ssrc-id> mslabel:"
		 *
		 * The second is incorrect (https://tools.ietf.org/html/rfc5576#section-4.1)
		 */
		rtpTransceiverInit.stream_ids.emplace_back("0");

		auto result = this->pc->AddTransceiver(
		  track, rtpTransceiverInit); // NOLINT(performance-unnecessary-value-param)

		if (!result.ok())
		{
			rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = nullptr;

			return transceiver;
		}

		return result.value();
	}

	std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> PeerConnection::GetSenders()
	{
		MSC_TRACE();

		return this->pc->GetSenders();
	}

	bool PeerConnection::RemoveTrack(rtc::scoped_refptr<webrtc::RtpSenderInterface> sender)
	{
		MSC_TRACE();

		auto result = this->pc->RemoveTrackOrError(sender);
		return result.ok();
	}

	using RTCStatsReport = rtc::scoped_refptr<const webrtc::RTCStatsReport>;
	class RTCStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback
	{
	public:
		using Callback = std::function<void(const RTCStatsReport&, webrtc::RTCError)>;

		RTCStatsCollectorCallback(Callback fn): callback(fn) {};

	private:
		/* Virtual methods inherited from webrtc::RTCStatsCollectorCallback. */
		void OnStatsDelivered(const RTCStatsReport& report) override {
			callback(report, webrtc::RTCError::OK());
		}
	private:
		Callback callback;
	};

	void PeerConnection::GetStats(StatsHandler callback)
	{
		MSC_TRACE();

		this->pc->signaling_thread()->PostTask([pc = this->pc, callback] {
			if (pc->signaling_state() == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
				callback(nullptr, webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE));
			} else {
				pc->GetStats(rtc::make_ref_counted<RTCStatsCollectorCallback>(callback).get());
			}
		});
	}

	void PeerConnection::GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface> selector, StatsHandler callback)
	{
		MSC_TRACE();

		this->pc->signaling_thread()->PostTask([pc = this->pc, selector, callback] {
			if (pc->signaling_state() == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
				callback(nullptr, webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE));
			} else if (!selector) {
				callback(nullptr, webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER));
			} else {
				pc->GetStats(selector, rtc::make_ref_counted<RTCStatsCollectorCallback>(callback));
			}
		});
	}

	void PeerConnection::GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector, StatsHandler callback)
	{
		MSC_TRACE();

		this->pc->signaling_thread()->PostTask([pc = this->pc, selector, callback] {
			if (pc->signaling_state() == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
				callback(nullptr, webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE));
			} else if (!selector) {
				callback(nullptr, webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER));
			} else {
				pc->GetStats(selector, rtc::make_ref_counted<RTCStatsCollectorCallback>(callback));
			}
		});
	}

	rtc::scoped_refptr<webrtc::DataChannelInterface> PeerConnection::CreateDataChannel(
	  const std::string& label, const webrtc::DataChannelInit* config)
	{
		MSC_TRACE();

		auto result = this->pc->CreateDataChannelOrError(label, config);

		if (result.ok())
		{
			MSC_DEBUG("Success creating data channel");
		}
		else
		{
			MSC_THROW_ERROR("Failed creating data channel");
		}

		return result.MoveValue();
	}

	/* PeerConnection::PrivateListener */

	/**
	 * Triggered when the SignalingState changed.
	 */
	void PeerConnection::PrivateListener::OnSignalingChange(
	  webrtc::PeerConnectionInterface::SignalingState newState)
	{
		MSC_TRACE();

		MSC_DEBUG("[newState:%s]", PeerConnection::signalingState2String[newState].c_str());
	}

	/**
	 * Triggered when media is received on a new stream from remote peer.
	 */
	void PeerConnection::PrivateListener::OnAddStream(
	  rtc::scoped_refptr<webrtc::MediaStreamInterface> /*stream*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when a remote peer closes a stream.
	 */
	void PeerConnection::PrivateListener::OnRemoveStream(
	  rtc::scoped_refptr<webrtc::MediaStreamInterface> /*stream*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when a remote peer opens a data channel.
	 */
	void PeerConnection::PrivateListener::OnDataChannel(
	  rtc::scoped_refptr<webrtc::DataChannelInterface> /*dataChannel*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when renegotiation is needed. For example, an ICE restart has begun.
	 */
	void PeerConnection::PrivateListener::OnRenegotiationNeeded()
	{
		MSC_TRACE();
	}

	/**
	 * Triggered any time the IceConnectionState changes.
	 *
	 * Note that our ICE states lag behind the standard slightly. The most
	 * notable differences include the fact that "failed" occurs after 15
	 * seconds, not 30, and this actually represents a combination ICE + DTLS
	 * state, so it may be "failed" if DTLS fails while ICE succeeds.
	 */
	void PeerConnection::PrivateListener::OnIceConnectionChange(
	  webrtc::PeerConnectionInterface::IceConnectionState newState)
	{
		MSC_TRACE();

		MSC_DEBUG("[newState:%s]", PeerConnection::iceConnectionState2String[newState].c_str());
	}

	/**
	 * Triggered any time the IceGatheringState changes.
	 */
	void PeerConnection::PrivateListener::OnIceGatheringChange(
	  webrtc::PeerConnectionInterface::IceGatheringState newState)
	{
		MSC_TRACE();

		MSC_DEBUG("[newState:%s]", PeerConnection::iceGatheringState2String[newState].c_str());
	}

	/**
	 * Triggered when a new ICE candidate has been gathered.
	 */
	void PeerConnection::PrivateListener::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
	{
		MSC_TRACE();

		std::string candidateStr;

		candidate->ToString(&candidateStr);

		MSC_DEBUG("[candidate:%s]", candidateStr.c_str());
	}

	/**
	 * Triggered when the ICE candidates have been removed.
	 */
	void PeerConnection::PrivateListener::OnIceCandidatesRemoved(
	  const std::vector<cricket::Candidate>& /*candidates*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when the ICE connection receiving status changes.
	 */
	void PeerConnection::PrivateListener::OnIceConnectionReceivingChange(bool /*receiving*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when a receiver and its track are created.
	 *
	 * Note: This is called with both Plan B and Unified Plan semantics. Unified
	 * Plan users should prefer OnTrack, OnAddTrack is only called as backwards
	 * compatibility (and is called in the exact same situations as OnTrack).
	 */
	void PeerConnection::PrivateListener::OnAddTrack(
	  rtc::scoped_refptr<webrtc::RtpReceiverInterface> /*receiver*/,
	  const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& /*streams*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when signaling indicates a transceiver will be receiving
	 *
	 * media from the remote endpoint. This is fired during a call to
	 * SetRemoteDescription. The receiving track can be accessed by:
	 * transceiver->receiver()->track() and its associated streams by
	 * transceiver->receiver()->streams().
	 *
	 * NOTE: This will only be called if Unified Plan semantics are specified.
	 * This behavior is specified in section 2.2.8.2.5 of the "Set the
	 * RTCSessionDescription" algorithm:
	 *   https://w3c.github.io/webrtc-pc/#set-description
	 */
	void PeerConnection::PrivateListener::OnTrack(
	  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> /*transceiver*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when signaling indicates that media will no longer be received on a
	 * track.
	 *
	 * With Plan B semantics, the given receiver will have been removed from the
	 * PeerConnection and the track muted.
	 * With Unified Plan semantics, the receiver will remain but the transceiver
	 * will have changed direction to either sendonly or inactive.
	 *   https://w3c.github.io/webrtc-pc/#process-remote-track-removal
	 */
	void PeerConnection::PrivateListener::OnRemoveTrack(
	  rtc::scoped_refptr<webrtc::RtpReceiverInterface> /*receiver*/)
	{
		MSC_TRACE();
	}

	/**
	 * Triggered when an interesting usage is detected by WebRTC.
	 *
	 * An appropriate action is to add information about the context of the
	 * PeerConnection and write the event to some kind of "interesting events"
	 * log function.
	 * The heuristics for defining what constitutes "interesting" are
	 * implementation-defined.
	 */
	void PeerConnection::PrivateListener::OnInterestingUsage(int /*usagePattern*/)
	{
		MSC_TRACE();
	}
} // namespace mediasoupclient
