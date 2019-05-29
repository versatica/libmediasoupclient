#define MSC_CLASS "scalabilityMode"
// #define MSC_LOG_DEV

#include "scalabilityMode.hpp"
#include "Logger.hpp"
#include <regex>
#include <string>

using json = nlohmann::json;

static const std::regex ScalabilityModeRegex(
  "^[LS]([1-9]\\d{0,1})T([1-9]\\d{0,1}).*", std::regex_constants::ECMAScript);

namespace mediasoupclient
{
json parseScalabilityMode(const std::string& scalabilityMode)
{
	/* clang-format off */
	json jsonScalabilityMode
	{
		{ "spatialLayers",  1 },
		{ "temporalLayers", 1 }
	};
	/* clang-format on */

	std::smatch match;

	std::regex_match(scalabilityMode, match, ScalabilityModeRegex);

	if (match.size() > 0)
	{
		try
		{
			jsonScalabilityMode["spatialLayers"]  = std::stoul(match[1].str());
			jsonScalabilityMode["temporalLayers"] = std::stoul(match[2].str());
		}
		catch (std::exception& e)
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
