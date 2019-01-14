#ifndef MSC_UTILS_HPP
#define MSC_UTILS_HPP

#include <cstdint> // uint32_t
#include <ctime>   // generator seed
#include <random>  // generators header
#include <set>
#include <sstream> // istringstream
#include <string>

namespace mediasoupclient
{
namespace Utils
{
	template<typename T>
	T getRandomInteger(T min, T max);
	std::string getRandomInteger(size_t len);

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

	inline std::string getRandomString(size_t len)
	{
		static std::vector<char> chars =
		{
			'0','1','2','3','4','5','6','7','8','9',
			'a','b','c','d','e','f','g','h','i','j',
			'k','l','m','n','o','p','q','r','s','t',
			'u','v','w','x','y','z',
			'A','B','C','D','E','F','G','H','I','J',
			'K','L','M','N','O','P','Q','R','S','T',
			'U','V','W','X','Y','Z'
		};

		// Seed with time.
		static unsigned int seed = time(nullptr);

		// Engine based on the Mersenne Twister 19937 (64 bits).
		static std::mt19937_64 rng(seed);

		// Uniform distribution for integers in the [min, max) range.
		std::uniform_int_distribution<std::string::size_type> dis(0, chars.size());

		std::string s;

    s.reserve(len);

    while(len--)
        s += chars[dis(rng)];

    return s;
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
} // namespace mediasoupclient

#endif
