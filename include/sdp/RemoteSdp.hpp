#ifndef MSC_REMOTESDP_HPP
#define MSC_REMOTESDP_HPP

#include "sdp/MediaSection.hpp"
#include <json.hpp>
#include <string>
#include <unordered_map>

namespace mediasoupclient
{
	namespace Sdp
	{
		class RemoteSdp
		{
		public:
			struct MediaSectionIdx
			{
				size_t idx;
				std::string reuseMid;
			};

		public:
			RemoteSdp(
			  const nlohmann::json& iceParameters,
			  const nlohmann::json& iceCandidates,
			  const nlohmann::json& dtlsParameters,
			  const nlohmann::json& sctpParameters);
			~RemoteSdp();

		public:
			Sdp::RemoteSdp::MediaSectionIdx GetNextMediaSectionIdx();
			void Send(
			  nlohmann::json& offerMediaObject,
			  const std::string& reuseMid,
			  nlohmann::json& offerRtpParameters,
			  nlohmann::json& answerRtpParameters,
			  const nlohmann::json* codecOptions);

			void SendSctpAssociation(nlohmann::json& offerMediaObject);
			void RecvSctpAssociation();

			void Receive(
			  const std::string& mid,
			  const std::string& kind,
			  const nlohmann::json& offerRtpParameters,
			  const std::string& streamId,
			  const std::string& trackId);
			void UpdateIceParameters(const nlohmann::json& iceParameters);
			void UpdateDtlsRole(const std::string& role);
			void DisableMediaSection(const std::string& mid);
			void CloseMediaSection(const std::string& mid);
			std::string GetSdp();

		private:
			void AddMediaSection(MediaSection* newMediaSection);
			void ReplaceMediaSection(MediaSection* newMediaSection, const std::string& reuseMid);
			void RegenerateBundleMids();

		protected:
			// Generic sending RTP parameters for audio and video.
			nlohmann::json rtpParametersByKind = nlohmann::json::object();
			// Transport remote parameters, including ICE parameters, ICE candidates,
			// DTLS parameteres and SCTP parameters.
			nlohmann::json iceParameters  = nlohmann::json::object();
			nlohmann::json iceCandidates  = nlohmann::json::object();
			nlohmann::json dtlsParameters = nlohmann::json::object();
			nlohmann::json sctpParameters = nlohmann::json::object();
			// MediaSection instances.
			std::vector<MediaSection*> mediaSections;
			// MediaSection indices indexed by MID.
			std::map<std::string, size_t> midToIndex;
			// First MID.
			std::string firstMid;
			// Generic sending RTP parameters for audio and video.
			nlohmann::json sendingRtpParametersByKind = nlohmann::json::object();
			// SDP global fields.
			nlohmann::json sdpObject = nlohmann::json::object();
		};
	} // namespace Sdp
} // namespace mediasoupclient

#endif
