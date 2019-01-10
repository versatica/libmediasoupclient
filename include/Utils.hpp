#ifndef MSC_UTILS_HPP
#define MSC_UTILS_HPP

#include <cstdint> // uint32_t
#include <ctime>   // generator seed
#include <random>  // generators header
#include <set>
#include <sstream> // istringstream

namespace Utils
{
	template<typename T>
	T getRandomInteger(T min, T max);

	std::vector<std::string> split(const std::string& s, char delimiter);

	/* Inline utils implementations */

	template<typename T>
	inline T getRandomInteger(T min, T max)
	{
		// Seed with time.
		static unsigned int seed = time(nullptr);

		// Engine based on the Mersenne Twister 19937 (64 bits).
		static std::mt19937_64 rng(seed);

		// Uniform distribution for integers in the [min, max) range.
		std::uniform_int_distribution<T> dis(min, max);

		return dis(rng);
	}

	inline std::vector<std::string> split(const std::string& s, char delimiter)
	{
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream tokenStream(s);
		while (std::getline(tokenStream, token, delimiter))
		{
			tokens.push_back(token);
		}
		return tokens;
	}
} // namespace Utils

#endif
