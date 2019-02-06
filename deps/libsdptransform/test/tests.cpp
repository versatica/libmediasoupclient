#define CATCH_CONFIG_RUNNER

#include "sdptransform.hpp"
#include "include/catch.hpp"

int main(int argc, char* argv[])
{
	int ret = Catch::Session().run(argc, argv);

	return ret;
}
