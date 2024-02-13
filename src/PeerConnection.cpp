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

	/* Instance methods. */

	PeerConnection::PeerConnection(
	  PeerConnection::PrivateListener* privateListener, const PeerConnection::Options* options)
	{
		MSC_TRACE();

		webrtc::PeerConnectionInterface::RTCConfiguration config;

		if (options != nullptr)
			config = options->config;

		// PeerConnection factory provided.
		if ((options != nullptr) && (options->factory != nullptr))
		{
			this->peerConnectionFactory =
			  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>(options->factory);
		}
		else
		{
			this->networkThread   = rtc::Thread::CreateWithSocketServer();
			this->signalingThread = rtc::Thread::Create();
			this->workerThread    = rtc::Thread::Create();

			this->networkThread->SetName("network_thread", nullptr);
			this->signalingThread->SetName("signaling_thread", nullptr);
			this->workerThread->SetName("worker_thread", nullptr);

			if (!this->networkThread->Start() || !this->signalingThread->Start() || !this->workerThread->Start())
			{
				MSC_THROW_INVALID_STATE_ERROR("thread start errored");
			}

			this->peerConnectionFactory = webrtc::CreatePeerConnectionFactory(
			  this->networkThread.get(),
			  this->workerThread.get(),
			  this->signalingThread.get(),
			  nullptr /*default_adm*/,
			  webrtc::CreateBuiltinAudioEncoderFactory(),
			  webrtc::CreateBuiltinAudioDecoderFactory(),
			  webrtc::CreateBuiltinVideoEncoderFactory(),
			  webrtc::CreateBuiltinVideoDecoderFactory(),
			  nullptr /*audio_mixer*/,
			  nullptr /*audio_processing*/);
		}

		// Set SDP semantics to Unified Plan.
		config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

		// Create the webrtc::Peerconnection.
		this->pc =
		  this->peerConnectionFactory->CreatePeerConnection(config, nullptr, nullptr, privateListener);
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
		  webrtc::ToString(error.type()).data(),
		  error.message());

		return false;
	}

	std::string PeerConnection::CreateOffer(
	  const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
	{
		MSC_TRACE();

		CreateSessionDescriptionObserver* sessionDescriptionObserver =
		  new rtc::RefCountedObject<CreateSessionDescriptionObserver>();

		auto future = sessionDescriptionObserver->GetFuture();

		this->pc->CreateOffer(sessionDescriptionObserver, options);

		return future.get();
	}

	std::string PeerConnection::CreateAnswer(
	  const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
	{
		MSC_TRACE();

		CreateSessionDescriptionObserver* sessionDescriptionObserver =
		  new rtc::RefCountedObject<CreateSessionDescriptionObserver>();

		auto future = sessionDescriptionObserver->GetFuture();

		this->pc->CreateAnswer(sessionDescriptionObserver, options);

		return future.get();
	}

	void PeerConnection::SetLocalDescription(PeerConnection::SdpType type, const std::string& sdp)
	{
		MSC_TRACE();

		webrtc::SdpParseError error;
		std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription;
		rtc::scoped_refptr<SetLocalDescriptionObserver> observer(
		  new rtc::RefCountedObject<SetLocalDescriptionObserver>());

		const auto& typeStr = sdpType2String[type];
		auto future         = observer->GetFuture();

		sessionDescription.reset(webrtc::CreateSessionDescription(typeStr, sdp, &error));
		if (sessionDescription == nullptr)
		{
			MSC_WARN(
			  "webrtc::CreateSessionDescription failed [%s]: %s",
			  error.line.c_str(),
			  error.description.c_str());

			observer->Reject(error.description);

			return future.get();
		}

		this->pc->SetLocalDescription(std::move(sessionDescription), observer);

		return future.get();
	}

	void PeerConnection::SetRemoteDescription(PeerConnection::SdpType type, const std::string& sdp)
	{
		MSC_TRACE();

		webrtc::SdpParseError error;
		std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription;
		rtc::scoped_refptr<SetRemoteDescriptionObserver> observer(
		  new rtc::RefCountedObject<SetRemoteDescriptionObserver>());

		const auto& typeStr = sdpType2String[type];
		auto future         = observer->GetFuture();

		sessionDescription.reset(webrtc::CreateSessionDescription(typeStr, sdp, &error));
		if (sessionDescription == nullptr)
		{
			MSC_WARN(
			  "webrtc::CreateSessionDescription failed [%s]: %s",
			  error.line.c_str(),
			  error.description.c_str());

			observer->Reject(error.description);

			return future.get();
		}

		this->pc->SetRemoteDescription(std::move(sessionDescription), observer);

		return future.get();
	}

	const std::string PeerConnection::GetLocalDescription()
	{
		MSC_TRACE();

		auto desc = this->pc->local_description();
		std::string sdp;

		desc->ToString(&sdp);

		return sdp;
	}

	const std::string PeerConnection::GetRemoteDescription()
	{
		MSC_TRACE();

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

		const auto result = this->pc->RemoveTrackOrError(sender);

		return result.ok();
	}

	json PeerConnection::GetStats()
	{
		MSC_TRACE();

		rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		  new rtc::RefCountedObject<RTCStatsCollectorCallback>());

		auto future = callback->GetFuture();

		this->pc->GetStats(callback.get());

		return future.get();
	}

	json PeerConnection::GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface> selector)
	{
		MSC_TRACE();

		rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		  new rtc::RefCountedObject<RTCStatsCollectorCallback>());

		auto future = callback->GetFuture();

		this->pc->GetStats(std::move(selector), callback);

		return future.get();
	}

	json PeerConnection::GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector)
	{
		MSC_TRACE();

		rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		  new rtc::RefCountedObject<RTCStatsCollectorCallback>());

		auto future = callback->GetFuture();

		this->pc->GetStats(std::move(selector), callback);

		return future.get();
	}

	rtc::scoped_refptr<webrtc::DataChannelInterface> PeerConnection::CreateDataChannel(
	  const std::string& label, const webrtc::DataChannelInit* config)
	{
		MSC_TRACE();

		const auto result =
		  this->pc->CreateDataChannelOrError(label, config);

		if (result.ok())
		{
			MSC_DEBUG("Success creating data channel");
		}
		else
		{
			MSC_THROW_ERROR("Failed creating data channel");
		}

		return result.value();
	}

	/* SetLocalDescriptionObserver */

	std::future<void> PeerConnection::SetLocalDescriptionObserver::GetFuture()
	{
		MSC_TRACE();

		return this->promise.get_future();
	}

	void PeerConnection::SetLocalDescriptionObserver::Reject(const std::string& error)
	{
		MSC_TRACE();

		this->promise.set_exception(std::make_exception_ptr(MediaSoupClientError(error.c_str())));
	}

	void PeerConnection::SetLocalDescriptionObserver::OnSetLocalDescriptionComplete(webrtc::RTCError error)
	{
		MSC_TRACE();

		if (!error.ok())
		{
			MSC_WARN(
					"webtc::SetLocalDescriptionObserver failure [%s:%s]",
					webrtc::ToString(error.type()).data(),
					error.message());

			auto message = std::string(error.message());

			this->Reject(message);
		}
		else
		{
			MSC_THROW_ERROR("Failed creating data channel");
			this->promise.set_value();
		}
	};

	/* SetRemoteDescriptionObserver */

	std::future<void> PeerConnection::SetRemoteDescriptionObserver::GetFuture()
	{
		MSC_TRACE();

		return this->promise.get_future();
	}

	void PeerConnection::SetRemoteDescriptionObserver::Reject(const std::string& error)
	{
		MSC_TRACE();

		this->promise.set_exception(std::make_exception_ptr(MediaSoupClientError(error.c_str())));
	}

	void PeerConnection::SetRemoteDescriptionObserver::OnSetRemoteDescriptionComplete(webrtc::RTCError error)
	{
		MSC_TRACE();

		if (!error.ok())
		{
			MSC_WARN(
					"webtc::SetRemoteDescriptionObserver failure [%s:%s]",
					webrtc::ToString(error.type()).data(),
					error.message());

			auto message = std::string(error.message());

			this->Reject(message);
		}
		else
		{
			this->promise.set_value();
		}
	};

	/* SetSessionDescriptionObserver */

	std::future<void> PeerConnection::SetSessionDescriptionObserver::GetFuture()
	{
		MSC_TRACE();

		return this->promise.get_future();
	}

	void PeerConnection::SetSessionDescriptionObserver::Reject(const std::string& error)
	{
		MSC_TRACE();

		this->promise.set_exception(std::make_exception_ptr(MediaSoupClientError(error.c_str())));
	}

	void PeerConnection::SetSessionDescriptionObserver::OnSuccess()
	{
		MSC_TRACE();

		this->promise.set_value();
	};

	void PeerConnection::SetSessionDescriptionObserver::OnFailure(webrtc::RTCError error)
	{
		MSC_TRACE();

		MSC_WARN(
		  "webtc::SetSessionDescriptionObserver failure [%s:%s]",
		  webrtc::ToString(error.type()).data(),
		  error.message());

		auto message = std::string(error.message());

		this->Reject(message);
	};

	/* CreateSessionDescriptionObserver */

	std::future<std::string> PeerConnection::CreateSessionDescriptionObserver::GetFuture()
	{
		MSC_TRACE();

		return this->promise.get_future();
	}

	void PeerConnection::CreateSessionDescriptionObserver::Reject(const std::string& error)
	{
		MSC_TRACE();

		this->promise.set_exception(std::make_exception_ptr(MediaSoupClientError(error.c_str())));
	}

	void PeerConnection::CreateSessionDescriptionObserver::OnSuccess(
	  webrtc::SessionDescriptionInterface* desc)
	{
		MSC_TRACE();

		// This callback should take the ownership of |desc|.
		std::unique_ptr<webrtc::SessionDescriptionInterface> ownedDesc(desc);

		std::string sdp;

		ownedDesc->ToString(&sdp);
		this->promise.set_value(sdp);
	};

	void PeerConnection::CreateSessionDescriptionObserver::OnFailure(webrtc::RTCError error)
	{
		MSC_TRACE();

		MSC_WARN(
		  "webtc::CreateSessionDescriptionObserver failure [%s:%s]",
		  webrtc::ToString(error.type()).data(),
		  error.message());

		auto message = std::string(error.message());

		this->Reject(message);
	}

	/* RTCStatsCollectorCallback */

	std::future<json> PeerConnection::RTCStatsCollectorCallback::GetFuture()
	{
		MSC_TRACE();

		return this->promise.get_future();
	}

	void PeerConnection::RTCStatsCollectorCallback::OnStatsDelivered(
	  const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
	{
		MSC_TRACE();

		std::string s = report->ToJson();

		// RtpReceiver stats JSON string is sometimes empty.
		if (s.empty())
			this->promise.set_value(json::array());
		else
			this->promise.set_value(json::parse(s));
	};

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
