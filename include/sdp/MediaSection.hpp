#ifndef MSC_MEDIA_SECTION_HPP
#define MSC_MEDIA_SECTION_HPP

#include <json.hpp>
#include <string>

namespace mediasoupclient
{
	namespace Sdp
	{
		class MediaSection
		{
		public:
			MediaSection()          = default;
			virtual ~MediaSection() = default;
			MediaSection(const nlohmann::json& iceParameters, const nlohmann::json& iceCandidates);

		public:
			std::string GetMid() const;
			bool IsClosed() const;
			nlohmann::json GetObject() const;
			void SetIceParameters(const nlohmann::json& iceParameters);
			void Disable();
			void Close();

		public:
			virtual void SetDtlsRole(const std::string& role) = 0;

		protected:
			nlohmann::json mediaObject = nlohmann::json::object();
		};

		class AnswerMediaSection : public MediaSection
		{
		public:
			AnswerMediaSection(
			  const nlohmann::json& iceParameters,
			  const nlohmann::json& iceCandidates,
			  const nlohmann::json& dtlsParameters,
			  const nlohmann::json& sctpParameters,
			  const nlohmann::json& offerMediaObject,
			  nlohmann::json& offerRtpParameters,
			  nlohmann::json& answerRtpParameters,
			  const nlohmann::json* codecOptions);

		public:
			void SetDtlsRole(const std::string& role) override;
		};

		class OfferMediaSection : public MediaSection
		{
		public:
			OfferMediaSection(
			  const nlohmann::json& iceParameters,
			  const nlohmann::json& iceCandidates,
			  const nlohmann::json& dtlsParameters,
			  const nlohmann::json& sctpParameters,
			  const std::string& mid,
			  const std::string& kind,
			  const nlohmann::json& offerRtpParameters,
			  const std::string& streamId,
			  const std::string& trackId);

		public:
			void SetDtlsRole(const std::string& role) override;
		};
	} // namespace Sdp
} // namespace mediasoupclient

#endif
