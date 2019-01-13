#ifndef MSC_VIDEO_LAYERS_HPP
#define MSC_VIDEO_LAYERS_HPP

#include <set>
#include <string>

namespace mediasoupclient
{
/* clang-format off */
static std::set<std::string> layers = {
	{ "low"    },
	{ "medium" },
	{ "high"   }
};
/* clang-format on */

inline bool isValidSpatialLayer(const std::string& layer)
{
	return layers.find(layer) != layers.end();
}
} // namespace mediasoupclient

#endif
