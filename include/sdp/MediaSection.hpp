#ifndef MSC_MEDIA_SECTION_HPP
#define MSC_MEDIA_SECTION_HPP

#include "json.hpp"
#include <string>

namespace mediasoupclient
{
namespace Sdp
{
	class MediaSection
	{
	public:
		MediaSection(){};
		MediaSection(const nlohmann::json& iceParameters, const nlohmann::json& iceCandidates);

	public:
		std::string GetMid() const;
		nlohmann::json GetObject() const;
		void SetIceParameters(const nlohmann::json& iceParameters);
		void Disable();

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
		  const std::string& mid,
		  const std::string& kind,
		  const nlohmann::json& offerRtpParameters,
		  const std::string& streamId,
		  const std::string& trackId);

	public:
		void SetDtlsRole(const std::string& role) override;
	};

	// MediaSection inline methods.

	inline std::string MediaSection::GetMid() const
	{
		return this->mediaObject["mid"].get<std::string>();
	}

	inline nlohmann::json MediaSection::GetObject() const
	{
		return this->mediaObject;
	}

	inline void MediaSection::SetIceParameters(const nlohmann::json& iceParameters)
	{
		this->mediaObject["iceUfrag"] = iceParameters["usernameFragment"];
		this->mediaObject["icePwd"]   = iceParameters["password"];
	}

	inline void MediaSection::Disable()
	{
		this->mediaObject["direction"] = "inactive";

		this->mediaObject.erase("ext");
		this->mediaObject.erase("ssrcs");
		this->mediaObject.erase("ssrcGroups");
		this->mediaObject.erase("simulcast");
		this->mediaObject.erase("rids");
	}

	// AnswerMediaSection inline methods.

	inline void AnswerMediaSection::SetDtlsRole(const std::string& role)
	{
		if (role == "client")
			this->mediaObject["setup"] = "active";
		else if (role == "server")
			this->mediaObject["setup"] = "passive";
		else if (role == "auto")
			this->mediaObject["setup"] = "actpass";
	}

	// OfferMediaSection inline methods.

	inline void OfferMediaSection::SetDtlsRole(const std::string& /* role */)
	{
		this->mediaObject["setup"] = "actpass";
	}
} // namespace Sdp
} // namespace mediasoupclient

#endif
