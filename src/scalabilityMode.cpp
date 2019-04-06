#define MSC_CLASS "scalabilityMode"
// #define MSC_LOG_DEV

#include "scalabilityMode.hpp"
#include "Exception.hpp"
#include <regex>
#include <string>

using json = nlohmann::json;

static const std::regex ScalabilityModeRegex("^L(\\d+)T(\\d+)");

namespace mediasoupclient
{
json parseScalabilityMode(const std::string& scalabilityMode)
{
	json jsonScalabilityMode;
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
			throw Exception(std::string("invalid scalabilityMode: ") + e.what());
		}
	}
	else
	{
		throw Exception(std::string("invalid scalabilityMode: ") + scalabilityMode);
	}

	return jsonScalabilityMode;
}
} // namespace mediasoupclient
