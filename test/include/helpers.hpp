#ifndef SDPTRANSFORM_HELPERS_HPP
#define	SDPTRANSFORM_HELPERS_HPP

#include <string>
#include <fstream>   // std::ifstream
#include <streambuf> // std::istreambuf_iterator
#include <stdexcept>

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

		content.assign(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());

		return content;
	}
}

#endif
