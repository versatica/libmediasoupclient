#ifndef MSC_TEST_HELPERS_HPP
#define MSC_TEST_HELPERS_HPP

#include <fstream> // std::ifstream
#include <stdexcept>
#include <streambuf> // std::istreambuf_iterator
#include <string>

namespace helpers
{
	inline std::string readFile(const char* file)
	{
		std::ifstream in(file);

		if (!in)
			throw std::invalid_argument("could not open file");

		std::string content;

		in.seekg(0, std::ios::end);
		content.reserve(in.tellg());
		in.seekg(0, std::ios::beg);

		content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		return content;
	}
} // namespace helpers

#endif
