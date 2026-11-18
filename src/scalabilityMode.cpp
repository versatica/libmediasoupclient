#define MSC_CLASS "scalabilityMode"

#include "scalabilityMode.hpp"
#include "Logger.hpp"
#include <regex>

using nlohmann::json;

// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
static const std::regex ScalabilityModeRegex(
  "^[LS]([1-9]\\d{0,1})T([1-9]\\d{0,1}).*", std::regex_constants::ECMAScript);

namespace mediasoupclient
{
	json parseScalabilityMode(const std::string& scalabilityMode)
	{
		MSC_TRACE();

		/* clang-format off */
	json jsonScalabilityMode
	{
		{ "spatialLayers",  1 },
		{ "temporalLayers", 1 }
	};
		/* clang-format on */

		std::smatch match;

		std::regex_match(scalabilityMode, match, ScalabilityModeRegex);

		if (!match.empty())
		{
			// NOTE: The regex always has these 2 groups when it matches.
			try
			{
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
				jsonScalabilityMode["spatialLayers"] = std::stoul(match[1].str());
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
				jsonScalabilityMode["temporalLayers"] = std::stoul(match[2].str());
			}
			catch (const std::exception& e)
			{
				MSC_WARN("invalid scalabilityMode: %s", e.what());
			}
		}
		else
		{
			MSC_WARN("invalid scalabilityMode: %s", scalabilityMode.c_str());
		}

		return jsonScalabilityMode;
	}
} // namespace mediasoupclient
