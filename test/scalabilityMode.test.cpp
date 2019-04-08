#include "Exception.hpp"
#include "catch.hpp"
#include "scalabilityMode.hpp"

using namespace mediasoupclient;

TEST_CASE("scalabilityMode", "[scalabilityMode]")
{
	SECTION("parses correctly")
	{
		nlohmann::json jsonScalabilityMode;

		REQUIRE_NOTHROW(jsonScalabilityMode = parseScalabilityMode("L1T3"));
		REQUIRE(jsonScalabilityMode["spatialLayers"].get<uint32_t>() == 1);
		REQUIRE(jsonScalabilityMode["temporalLayers"].get<uint32_t>() == 3);

		REQUIRE_NOTHROW(jsonScalabilityMode = parseScalabilityMode("L30T3"));
		REQUIRE(jsonScalabilityMode["spatialLayers"].get<uint32_t>() == 30);
		REQUIRE(jsonScalabilityMode["temporalLayers"].get<uint32_t>() == 3);

		REQUIRE_NOTHROW(jsonScalabilityMode = parseScalabilityMode("L1T6"));
		REQUIRE(jsonScalabilityMode["spatialLayers"].get<uint32_t>() == 1);
		REQUIRE(jsonScalabilityMode["temporalLayers"].get<uint32_t>() == 6);
	}

	SECTION("return deault layers if input is incorrect")
	{
		nlohmann::json jsonScalabilityMode;

		REQUIRE_NOTHROW(jsonScalabilityMode = parseScalabilityMode("1T3"));
		REQUIRE(jsonScalabilityMode["spatialLayers"].get<uint32_t>() == 1);
		REQUIRE(jsonScalabilityMode["temporalLayers"].get<uint32_t>() == 1);
	}
}
