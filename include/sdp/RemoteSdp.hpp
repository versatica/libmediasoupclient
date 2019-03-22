#ifndef MSC_REMOTESDP_HPP
#define MSC_REMOTESDP_HPP

#include "sdp/MediaSection.hpp"
#include "json.hpp"
#include <map>
#include <string>

namespace mediasoupclient
{
namespace Sdp
{
	class RemoteSdp
	{
	public:
		RemoteSdp(
		  const nlohmann::json& iceParameters,
		  const nlohmann::json& iceCandidates,
		  const nlohmann::json& dtlsParameters);

	public:
		void Send(
		  nlohmann::json& offerMediaObject,
		  nlohmann::json& offerRtpParameters,
		  nlohmann::json& answerRtpParameters,
		  const nlohmann::json* codecOptions);
		void Receive(
		  const std::string& mid,
		  const std::string& kind,
		  const nlohmann::json& offerRtpParameters,
		  const std::string& streamId,
		  const std::string& trackId);
		void UpdateIceParameters(const nlohmann::json& iceParameters);
		void UpdateDtlsRole(const std::string& role);
		void DisableMediaSection(const std::string& mid);
		std::string GetSdp();

	private:
		void AddMediaSection(MediaSection* mediaSection);

	protected:
		// Generic sending RTP parameters for audio and video.
		nlohmann::json rtpParametersByKind = nlohmann::json::object();

		// Transport remote parameters, including ICE parameters, ICE candidates
		// and DTLS parameteres.
		nlohmann::json iceParameters  = nlohmann::json::object();
		nlohmann::json iceCandidates  = nlohmann::json::object();
		nlohmann::json dtlsParameters = nlohmann::json::object();

		// MediaSection instances indexed by MID.
		std::unordered_map<std::string, MediaSection*> mediaSections;

		// Generic sending RTP parameters for audio and video.
		nlohmann::json sendingRtpParametersByKind = nlohmann::json::object();

		// SDP global fields.
		nlohmann::json sdpObject = nlohmann::json::object();
	};
} // namespace Sdp
} // namespace mediasoupclient

#endif
